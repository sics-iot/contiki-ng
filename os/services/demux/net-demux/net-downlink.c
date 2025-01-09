/*
 * Copyright (c) 2019, Inria.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The uplink of the standalone RPL border router is implemented with
 * the SLIP I/F.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "contiki.h"

#include "lib/ringbufindex.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/sicslowpan.h"
#include "net/netstack.h"
#include "net/packetbuf.h"

#include "sys/log.h"

#include "net-downlink.h"

#define LOG_MODULE "demux-down"
#define LOG_LEVEL LOG_LEVEL_IPV6

#ifdef NET_DOWNLINK_CONF_SLIP_TX_BUF_NUM
#define NET_DOWNLINK_SLIP_TX_BUF_NUM NET_DOWNLINK_CONF_SLIP_TX_BUF_NUM
#else
/* need PACKETBUF_SIZE * 10 to handle the IPv6 minimum MTU */
#define NET_DOWNLINK_SLIP_TX_BUF_NUM 16
#endif /* NET_DOWNLINK_CONF_SLIP_TX_BUF_NUM */

#define DISABLE_FLOW_CONTROL false
#define DISABLE_SOFTWARE_FLOW_CONTROL false
#define DEFAULT_BAUD_RATE 115200
#define DEFAULT_SERIAL_TCP_REMOTE_HOST "localhost"
#define INVALID_SERIAL_TCP_PORT NULL
#define DEFAULT_SERIAL_DEVICE_PATH "/dev/ttyUSB0"
#define DEFAULT_MIN_INTERPACKET_GAP_MS 10

#define INVALID_FD -1

#define SLIP_END     0300
#define SLIP_ESC     0333
#define SLIP_ESC_END 0334
#define SLIP_ESC_ESC 0335

#define PRINT_STDERR(...) fprintf(stderr, __VA_ARGS__)

extern int contiki_argc;
extern char **contiki_argv;
extern const struct network_driver sicslowpan_driver;

static bool config_flow_control_enabled;
static bool config_software_flow_control_enabled;
static uint32_t config_baud_rate_const;
static const char *config_serial_tcp_remote_host;
static const char *config_serial_tcp_remote_port;
static const char *config_serial_device_path;
static uint16_t config_min_interpacket_gap_ms;

static linkaddr_t peer_mac_addr;
static int serial_fd = INVALID_FD;

static struct ringbufindex serial_tx_ringbuf_index;
static struct {
  /*
   * packetbuf should be large enough to store a packet all the octets
   * of which are escaped. Each packet is surrounded with SLIP_END,
   * which consumes 2 octets in total.
   */
  uint8_t packet_buf[(PACKETBUF_SIZE * 2 + 3) / 4 * 4 + 2];
  uint16_t packet_len;
} serial_tx_ringbuf[NET_DOWNLINK_SLIP_TX_BUF_NUM];

static uint32_t serial_rxbuf_aligned[(UIP_BUFSIZE + 3) / 4];
static uint8_t *serial_rxbuf = (uint8_t *)serial_rxbuf_aligned;
static uint16_t serial_rxbuf_len;
static const uint16_t serial_rxbuf_size = sizeof(serial_rxbuf_aligned);

static const struct {
  uint32_t baud_rate;
  speed_t baud_rate_const;
} baud_rate_table[] = {
  { 9600, B9600 },
  { 19200, B19200 },
  { 38400, B38400 },
  { 57600, B57600 },
  { 115200, B115200 },
  { 0, B0 }, /* invalid baud rate */
};

static void print_usage_and_exit(int status);
static speed_t get_baud_rate_const(uint32_t baud_rate);
static void set_default_config(void);
static int parse_peer_mac_addr(const char *mac_addr_str);
static void update_config_with_contiki_args(void);

static int serial_open_tcp_connection(void);
static int serial_open_device(void);
static int serial_fd_set(fd_set *rset, fd_set *wset);
static void serial_fd_handle(fd_set *rset, fd_set *wset);
static void serial_clear_rxbuf(void);
static void serial_handle_rx(void);

static void net_downlink_input(void);

/*---------------------------------------------------------------------------*/
static void
print_usage_and_exit(int status)
{
  PRINT_STDERR("\n");
  PRINT_STDERR("usage: %s ", contiki_argv[0]);
  PRINT_STDERR("[-H] "
               "[-X] "
               "[-B BAUD_RATE] "
               "[-p TCP_PORT] "
               "[-s SERIAL_DEV_PATH] "
               "[-d MIN_IPG_MS] MAC_ADDR_OF_DOWNLINK_PEER"
               "\n\n");
  PRINT_STDERR("-----------------------------------------------------------\n");
  PRINT_STDERR("  -h                  show this help message and exit\n");
  PRINT_STDERR("  -H                  enable flow control\n");
  PRINT_STDERR("  -X                  enable software flow control (XON/XOFF)\n");
  PRINT_STDERR("  -B BAUD_RATE        set baud rate (default %d)\n",
               DEFAULT_BAUD_RATE);
  PRINT_STDERR("  -a TCP_HOST         set remote host for serial-over-TCP"
               " (default %s)\n", DEFAULT_SERIAL_TCP_REMOTE_HOST);
  PRINT_STDERR("  -p TCP_PORT         set port and enable serial-over-TCP\n");
  PRINT_STDERR("  -s SERIAL_DEV_PATH  set serial device path (default %s)\n",
               DEFAULT_SERIAL_DEVICE_PATH);
  PRINT_STDERR("  -d MIN_IPG_MS       set min interpacket gap (default %u)\n",
               DEFAULT_MIN_INTERPACKET_GAP_MS);
  PRINT_STDERR("\n");
  exit(status);
}
/*---------------------------------------------------------------------------*/
static speed_t
get_baud_rate_const(uint32_t baud_rate)
{
  speed_t ret = B0; /* not found */
  for(int i = 0; baud_rate_table[i].baud_rate > 0; i++) {
    if(baud_rate_table[i].baud_rate == baud_rate) {
      ret = baud_rate_table[i].baud_rate_const;
      break;
    }
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
static void
set_default_config(void)
{
  config_flow_control_enabled = DISABLE_FLOW_CONTROL;
  config_software_flow_control_enabled = DISABLE_SOFTWARE_FLOW_CONTROL;
  config_baud_rate_const = get_baud_rate_const(DEFAULT_BAUD_RATE);
  config_serial_tcp_remote_host = DEFAULT_SERIAL_TCP_REMOTE_HOST;
  config_serial_tcp_remote_port = INVALID_SERIAL_TCP_PORT;
  config_serial_device_path = DEFAULT_SERIAL_DEVICE_PATH;
  config_min_interpacket_gap_ms = DEFAULT_MIN_INTERPACKET_GAP_MS;
}
/*---------------------------------------------------------------------------*/
#define MAX_MAC_ADDR_STR_LEN (2 * LINKADDR_SIZE + 1 * (LINKADDR_SIZE - 1) + 1)
static int
parse_peer_mac_addr(const char *mac_addr_str)
{
  /* mac_addr_str is expected to be a null-terminated string */
  enum { SUCCESS = 0, FAILURE = -1 } ret;
  const char *delim = ":";
  char buf[MAX_MAC_ADDR_STR_LEN];

  if(strlen(mac_addr_str) > sizeof(buf) - 1) {
    /*
     * mac_addr_str is too long; the last byte of buf needs to be
     * reserved for NULL
     */
    PRINT_STDERR("MAC_ADDR_OF_DOWNLINK_PEER: "
                 "the input string, \"%s\", is too long\"\n", mac_addr_str);
    ret = FAILURE;
  } else {
    int i;
    char *hex_str;

    (void)strncpy(buf, mac_addr_str, sizeof(buf));
    i = 0;
    hex_str = strtok(buf, delim);
    while(i < LINKADDR_SIZE && hex_str != NULL) {
      peer_mac_addr.u8[i] = (unsigned char)strtol(hex_str, NULL, 16);
      i++;
      hex_str = strtok(NULL, delim);
    }
    if(i == LINKADDR_SIZE) {
      LOG_INFO("Set peer's MAC address to ");
      LOG_INFO_LLADDR(&peer_mac_addr);
      LOG_INFO_("\n");
      ret = SUCCESS;
    } else {
      PRINT_STDERR("MAC_ADDR_OF_DOWNLINK_PEER: invalid format\n");
      ret = FAILURE;
    }
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
static void
update_config_with_contiki_args(void)
{
  int c;
  speed_t baud_rate_const;
  int tcp_port;
  int fd;
  int min_interpacket_gap_ms;

  while((c = getopt(contiki_argc, contiki_argv, "HXB:a:p:s:d:h")) != -1) {
    switch(c) {
    case 'H':
      config_flow_control_enabled = true;
      break;
    case 'X':
      config_software_flow_control_enabled = true;
      break;
    case 'B':
      baud_rate_const = get_baud_rate_const(atoi(optarg));
      if(baud_rate_const == B0) {
        PRINT_STDERR("\n");
        PRINT_STDERR("ERROR: Invalid baud rate %s\n", optarg);
        PRINT_STDERR("       Supported baud rates are:");
        for(int i = 0; baud_rate_table[i].baud_rate > 0; i++) {
          PRINT_STDERR(" %u", baud_rate_table[i].baud_rate);
        }
        PRINT_STDERR("\n");
        print_usage_and_exit(1);
      } else {
        LOG_DBG("Baud rate is set to %s\n", optarg);
        config_baud_rate_const = baud_rate_const;
      }
      break;
    case 'a':
      /*
       * we don't check if the specified string is a valid IP address
       * nor a resolvable name
       */
      LOG_DBG("Remote host for serial-over-TCP is set to %s\n", optarg);
      config_serial_tcp_remote_host = optarg;
      break;
    case 'p':
      tcp_port = atoi(optarg);
      if(tcp_port < 1 || tcp_port > 65535) {
        /* invalid TCP port */
        PRINT_STDERR("\n");
        PRINT_STDERR("ERROR: Invalid TCP port %s\n", optarg);
        print_usage_and_exit(1);
      } else {
        LOG_DBG("Serial-over TCP is enabled with port %s\n", optarg);
        config_serial_tcp_remote_port = optarg;
      }
      break;
    case 's':
      if(strlen(optarg) < strlen("/dev/") ||
         strstr(optarg, "/dev/") != optarg) {
        PRINT_STDERR("\n");
        PRINT_STDERR("ERROR: Invalid path for a serial device, \"%s\"\n",
                     optarg);
        PRINT_STDERR("       The given path should start with \"/dev/\"\n");
        print_usage_and_exit(1);
      } else if((fd = open(optarg, O_RDWR | O_NONBLOCK)) < 0) {
        /* cannot open the specified (device) file */
        PRINT_STDERR("\n");
        PRINT_STDERR("ERROR: Cannot open the serial device, \"%s\"\n", optarg);
        print_usage_and_exit(1);
      } else {
        close(fd); /* we will open the serial device later, again */
        LOG_DBG("Serial device is set to %s\n", optarg);
        config_serial_device_path = optarg;
      }
      break;
    case 'd':
      min_interpacket_gap_ms = atoi(optarg);
      if(min_interpacket_gap_ms < 0 || min_interpacket_gap_ms > 1000) {
        PRINT_STDERR("\n");
        PRINT_STDERR("ERROR: Invalid minimum interpacet gap, %s ms\n",
                     optarg);
        PRINT_STDERR("       Acceptable range is [0, 1000] ms\n");
        print_usage_and_exit(1);
      } else {
        LOG_DBG("Minimum interpacket gap is set to %s ms\n", optarg);
        config_min_interpacket_gap_ms = min_interpacket_gap_ms;
      }
      break;
    case '?':
    case 'h':
    default:
      print_usage_and_exit(1);
      break; /* shouldn't come here */
    }
  }
  if((contiki_argc - optind) == 0) {
    PRINT_STDERR("\n");
    PRINT_STDERR("Error: MAC_ADDR_OF_DOWNLINK_PEER needs to be specified.\n");
    PRINT_STDERR("\n");
    exit(1);
  } else if((contiki_argc - optind) == 1) {
    if(parse_peer_mac_addr(contiki_argv[optind]) < 0) {
      /* parse error */
      PRINT_STDERR("\n");
      PRINT_STDERR("Error: MAC_ADDR_OF_DOWNLINK_PEER must be "
                   "in the colon-separated form like "
                   "\"02:11:22:33:44:55:66:77\"\n");
      PRINT_STDERR("\n");
      exit(1);
    } else {
      /* command-line arguments are processed correctly */
    }
  } else {
    print_usage_and_exit(1);
  }
}
/*---------------------------------------------------------------------------*/
static int
serial_open_tcp_connection(void)
{
  int gai_ret;
  struct addrinfo hints, *res;
  int s;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  if((gai_ret = getaddrinfo(config_serial_tcp_remote_host,
                            config_serial_tcp_remote_port,
                            &hints, &res)) != 0) {
    PRINT_STDERR("\n");
    PRINT_STDERR("getaddrinfo() failed with %s: %s\n\n",
                 config_serial_tcp_remote_host,
                 gai_strerror(gai_ret));
    exit(1);
  }

  s = INVALID_FD;
  for(struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
    s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if(s < 0) {
      /* failed to open a socket; try the next rp */
      LOG_WARN("socket() failed: %s\n", strerror(errno));
      s = INVALID_FD;
    } else if(connect(s, rp->ai_addr, rp->ai_addrlen) < 0) {
      /* failed to connect to the host throught the socket; try the next rp */
      LOG_WARN("connect() failed: %s\n", strerror(errno));
      close(s);
      s = INVALID_FD;
    } else {
      /* the socket is ready */
      break;
    }
  }
  freeaddrinfo(res);

  if(s == INVALID_FD) {
    PRINT_STDERR("\n");
    PRINT_STDERR("Failed to set up a socket for serial-over-TCP\n\n");
    exit(1);
  } else {
    fcntl(s, F_SETFL, O_NONBLOCK);
    LOG_INFO("Connected to serial-over-TCP\n");
  }

  return s;
}
/*---------------------------------------------------------------------------*/
static int
serial_open_device(void)
{
  int fd;
  struct termios termios;

  if((fd = open(config_serial_device_path, O_RDWR | O_NONBLOCK)) < 0) {
    PRINT_STDERR("\n");
    PRINT_STDERR("Failed to open \"%s\": %s\n\n",
                 config_serial_device_path, strerror(errno));
    exit(1);
  } else {
    if(tcflush(fd, TCIOFLUSH ) < 0) {
      LOG_WARN("tcflush() failed: %s\n", strerror(errno));
      /* keep going */
    }

    /* set it to raw mode */
    cfmakeraw(&termios);
    /* we'll use select() to monitor fd; set "polling read" */
    termios.c_cc[VMIN] = 0;
    termios.c_cc[VTIME] = 0;
    if(config_flow_control_enabled) {
      termios.c_cflag |= CRTSCTS;
    } else {
      termios.c_cflag &= ~CRTSCTS;
    }
    /* flow control stuff */
    if(config_software_flow_control_enabled) {
      termios.c_cflag |= IXON | IXOFF;
    } else {
      termios.c_cflag &= ~(IXON | IXOFF);
    }
    if(tcsetattr(fd, TCSAFLUSH, &termios) < 0) {
      PRINT_STDERR("\n");
      PRINT_STDERR("Failed to make the serial device raw: %s\n\n",
                   strerror(errno));
      exit(1);
    }
    /* baud rate */
    if(cfsetispeed(&termios, config_baud_rate_const) < 0 ||
       cfsetospeed(&termios, config_baud_rate_const) < 0) {
      PRINT_STDERR("\n");
      PRINT_STDERR("Failed to set baud rate: %s\n\n", strerror(errno));
      exit(1);
    }
    /* finalize */
    if(tcsetattr(fd, TCSAFLUSH, &termios) < 0) {
      PRINT_STDERR("\n");
      PRINT_STDERR("Failed to initialize the serial device: %s\n\n",
                   strerror(errno));
      exit(1);
    }
  }
  return fd;
}
/*---------------------------------------------------------------------------*/
static int
serial_fd_set(fd_set *rset, fd_set *wset)
{
  FD_SET(serial_fd, rset);
  FD_SET(serial_fd, wset);
  return 1;
}
/*---------------------------------------------------------------------------*/
static void
serial_fd_handle(fd_set *rset, fd_set *wset)
{
  if(FD_ISSET(serial_fd, rset)) {
    serial_handle_rx();
  }

  if(FD_ISSET(serial_fd, wset)) {
    int idx;
    int write_len;
    if((idx = ringbufindex_peek_get(&serial_tx_ringbuf_index)) >= 0) {
      LOG_DBG("Writing %u bytes to SLIP (ringbufindex: %d)\n",
              serial_tx_ringbuf[idx].packet_len, idx);
      if((write_len = write(serial_fd,
                            serial_tx_ringbuf[idx].packet_buf,
                            serial_tx_ringbuf[idx].packet_len)) < 0) {
        LOG_ERR("write() to the serial device failed (%s); will retry\n",
                strerror(errno));
      } else if(write_len == serial_tx_ringbuf[idx].packet_len) {
        /* the whole data at idx is written successfully */
        serial_tx_ringbuf[idx].packet_len = 0;
        (void)ringbufindex_get(&serial_tx_ringbuf_index);
        if(config_min_interpacket_gap_ms > 0) {
          (void)usleep(config_min_interpacket_gap_ms * 1000);
        } else {
          /* do nothing */
        }
      } else {
        /*
         * data at ids is partially written; the remaining will be
         * processed later
         */
        assert(write_len < serial_tx_ringbuf[idx].packet_len);
        memmove(serial_tx_ringbuf[idx].packet_buf,
                serial_tx_ringbuf[idx].packet_buf + write_len,
                serial_tx_ringbuf[idx].packet_len - write_len);
        serial_tx_ringbuf[idx].packet_len -= write_len;
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
serial_clear_rxbuf(void)
{
  if(serial_rxbuf_len > 0) {
    LOG_WARN("Drop a buffered rx packet (%u bytes)\n", serial_rxbuf_len);
    serial_rxbuf_len = 0;
  }
}
/*---------------------------------------------------------------------------*/
static void
serial_handle_rx(void)
{
  static enum {
    STATE_WAITING_FOR_DELIMITER,
    STATE_WAITING_FOR_FIRST_BYTE,
    STATE_WAITING_FOR_SLIP_ESC_END,
    STATE_RECEIVING_PACKET,
    STATE_EXPECTING_PRINTABLE_BYTE,
  } state = STATE_WAITING_FOR_FIRST_BYTE;
  const char *hop_1_log_header = "6LR --";
  uint8_t byte;

  while(read(serial_fd, &byte, sizeof(byte)) > 0) {
    if(state == STATE_WAITING_FOR_DELIMITER) {
      if(byte == SLIP_END) {
        state = STATE_WAITING_FOR_FIRST_BYTE;
      } else {
        /* ignore this byte */
      }
    } else if(state == STATE_WAITING_FOR_FIRST_BYTE) {
      if(byte == SLIP_END) {
        /*
         * do nothing; we expect the next one is the first byte of a
         * new messsage
         */
      } else if(byte == '!' || byte == '?') {
        /* this seems a serial command, which is not supported; ignore it */
        state = STATE_WAITING_FOR_DELIMITER;
      } else if (byte == SLIP_ESC) {
        /*
         * The first fragment of a full IPv6 packet may have 0xC0, the
         * same value as SLIP_END, in its first byte, which should be
         * escaped with SLIP_ESC. If the following byte is
         * SLIP_ESC_END, the receiving bytes is the first fragment of
         * a full IPv6 packet. Otherwise, it is considered as garbage.
         */
        state = STATE_WAITING_FOR_SLIP_ESC_END;
      } else if(byte == SICSLOWPAN_DISPATCH_HC1 ||
                ((byte & SICSLOWPAN_DISPATCH_IPHC_MASK) ==
                 SICSLOWPAN_DISPATCH_IPHC) ||
                ((byte & SICSLOWPAN_DISPATCH_FRAG_MASK) ==
                 SICSLOWPAN_DISPATCH_FRAG1) ||
                ((byte & SICSLOWPAN_DISPATCH_FRAG_MASK) ==
                 SICSLOWPAN_DISPATCH_FRAGN)) {
         /*
         * this seems a compressed IPv6 packet (RFC 4944, RFC 6282);
         * save it into rxbuf
         */
        serial_clear_rxbuf();
        serial_rxbuf[serial_rxbuf_len] = byte;
        serial_rxbuf_len++;
        state = STATE_RECEIVING_PACKET;
      } else if(byte == '\r') {
        /*
         * Debug output by cc2538 starts with '\r' when
         * DBG_CONF_SLIP_MUX is enabled. See arch/cpu/cc2538/dbg.c.
         */
        printf("%s ", hop_1_log_header);
        state = STATE_EXPECTING_PRINTABLE_BYTE;
      } else if(isprint(byte) == 0) {
        /* the first character is NOT printable; consider it as garbage */
        state = STATE_WAITING_FOR_DELIMITER;
      } else {
        /* if all the following octets are printable, take it a log message */
        printf("%s %c", hop_1_log_header, byte);
        state = STATE_EXPECTING_PRINTABLE_BYTE;
      }
    } else if(state == STATE_WAITING_FOR_SLIP_ESC_END) {
      if(byte == SLIP_ESC_END) {
        /* this byte should be the escaped dispatch value of FRAG1 */
        serial_clear_rxbuf();
        serial_rxbuf[0] = SLIP_ESC;
        serial_rxbuf[1] = SLIP_ESC_END;
        serial_rxbuf_len += 2;
        state = STATE_RECEIVING_PACKET;
      } else {
        /* this is garbage; ignore it */
        state = STATE_WAITING_FOR_DELIMITER;
      }
    } else if(state == STATE_RECEIVING_PACKET) {
      if(byte == SLIP_END) {
        net_downlink_input();
        state = STATE_WAITING_FOR_FIRST_BYTE;
      } else if(serial_rxbuf_len == serial_rxbuf_size){
        LOG_WARN("Packet too big; drop rx packet\n");
        serial_clear_rxbuf();
        state = STATE_WAITING_FOR_DELIMITER;
      } else {
        assert(serial_rxbuf_len < serial_rxbuf_size);
        serial_rxbuf[serial_rxbuf_len] = byte;
        serial_rxbuf_len++;
      }
    } else {
      assert(state == STATE_EXPECTING_PRINTABLE_BYTE);
      if(byte == SLIP_END) {
        printf("\n");
        state = STATE_WAITING_FOR_FIRST_BYTE;
      } else if(byte == '\n') {
        printf("\n");
        state = STATE_WAITING_FOR_FIRST_BYTE;
      } else if(isprint(byte) == 0) {
        /*
         * the received byte may be broken or, the entire message may
         * not be a log message in the first place
         */
        printf("\n");
        state = STATE_WAITING_FOR_DELIMITER;
      } else {
        printf("%c", byte);
      }
    }
  }
}
/*---------------------------------------------------------------------------*/
void
net_downlink_slip_send(mac_callback_t sent_callback, void *ptr)
{
  int idx;
  int status;
  int transmissions;

  if((idx = ringbufindex_peek_put(&serial_tx_ringbuf_index)) < 0) {
    LOG_ERR("No TX buffer for a new packet; drop it\n");
    status = MAC_TX_ERR;
    transmissions = 0;
  } else {
    uint8_t *packet_ptr = packetbuf_hdrptr();
    uint16_t packet_len = packetbuf_totlen();
    uint8_t *write_ptr = serial_tx_ringbuf[idx].packet_buf;

    LOG_DBG("Sending a packet to SLIP "
            "(%u bytes, ringbufindex: %d)\n", packet_len, idx);

    *write_ptr = SLIP_END;
    write_ptr++;
    for(int i = 0; i < packet_len; i++, write_ptr++) {
      if(packet_ptr[i] == SLIP_END) {
        *write_ptr = SLIP_ESC;
        write_ptr++;
        *write_ptr = SLIP_ESC_END;
      } else if(packet_ptr[i] == SLIP_ESC) {
        *write_ptr = SLIP_ESC;
        write_ptr++;
        *write_ptr = SLIP_ESC_ESC;
      } else {
        *write_ptr = packet_ptr[i];
      }
    }
    *write_ptr = SLIP_END;
    write_ptr++;

    serial_tx_ringbuf[idx].packet_len = (write_ptr -
                                         serial_tx_ringbuf[idx].packet_buf);

    if(ringbufindex_put(&serial_tx_ringbuf_index) < 0) {
      LOG_ERR("ringbufindex_put() failed for idx: %d\n", idx);
      status = MAC_TX_ERR;
      transmissions = 0;
    } else {
      status = MAC_TX_OK;
      transmissions = 1;
    }
  }

  if(sent_callback) {
    sent_callback(ptr, status, transmissions);
  }
}
/*---------------------------------------------------------------------------*/
static void
net_downlink_input(void)
{
  if(serial_fd == INVALID_FD) {
    /* not ready; nothing to do */
  } else if(serial_rxbuf_len > 0) {
    uint8_t *dataptr;
    uint16_t datalen;
    packetbuf_clear();
    dataptr = packetbuf_dataptr();
    datalen = 0;
    for(int i = 0; i < serial_rxbuf_len; i++, datalen++) {
      if(serial_rxbuf[i] == SLIP_ESC) {
        if(serial_rxbuf[i + 1] == SLIP_ESC_END) {
          dataptr[datalen] = SLIP_END;
        } else if(serial_rxbuf[i + 1] == SLIP_ESC_ESC) {
          dataptr[datalen] = SLIP_ESC;
        } else {
          LOG_WARN("Invalid byte (%02X) following SLIP_ESC\n",
                   serial_rxbuf[i + 1]);
          dataptr[datalen] = serial_rxbuf[i + 1];
        }
        i++; /* increment by one for SLIP_ESC */
      } else {
        dataptr[datalen] = serial_rxbuf[i];
      }
    }
    assert(packetbuf_hdrlen() == 0);
    /* we should receive only unicast frames to us */
    packetbuf_set_datalen(datalen);
    packetbuf_set_addr(PACKETBUF_ADDR_SENDER, &peer_mac_addr);
    packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, &linkaddr_node_addr);
    sicslowpan_driver.input();
  } else {
    /* we may have received two consecutive SLIP_ENDs; do nothing */
  }
  serial_rxbuf_len = 0;
}
/*---------------------------------------------------------------------------*/
void
net_downlink_init(void)
{
  static const struct select_callback serial_fd_select_callback = {
    serial_fd_set,
    serial_fd_handle
  };

  LOG_INFO("Initialize SLIP for the downlink\n");

  set_default_config();
  update_config_with_contiki_args();

  if(config_serial_tcp_remote_port == INVALID_SERIAL_TCP_PORT) {
    serial_fd = serial_open_device();
  } else {
    serial_fd = serial_open_tcp_connection();
  }
  select_set_callback(serial_fd, &serial_fd_select_callback);

  ringbufindex_init(&serial_tx_ringbuf_index, NET_DOWNLINK_SLIP_TX_BUF_NUM);
  sicslowpan_driver.init();
}
/*---------------------------------------------------------------------------*/
void
net_downlink_output(const linkaddr_t *dest)
{
  if(serial_fd == INVALID_FD) {
    /* not ready; nothing to do */
  } else if(sicslowpan_driver.output(dest) == 0) {
    LOG_ERR("Failed to compress the IPv6 header; drop tx packet\n");
  } else {
    /* done */
  }
}
/*---------------------------------------------------------------------------*/
bool
net_downlink_is_peer_linkaddr(const linkaddr_t *linkaddr)
{
  return linkaddr_cmp(linkaddr, &peer_mac_addr);
}
/*---------------------------------------------------------------------------*/
