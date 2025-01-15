/**
 * \file
 *         Simple contiki-ng program, which writes and reads data from the
 *         nrf52840 flash memory via nvmc (non volatile memory controller)
 *         Example based on: https://infocenter.nordicsemi.com/index.jsp?topic=%2Fsdk_nrf5_v16.0.0%2Flib_fstorage.html
 *
 *         This example will always update the same memory location, thus
 *         we need to erase the flash page everytime before we update the
 *         memory location.
 *
 * \author
 *         Andreas Karner <andreas.karner[at]student.tugraz.at
 */

#include "contiki.h"
#include <stdio.h>

// makefile will source them if NRF52840_USE_NVMC_FLASH is set
#include <nrf_fstorage.h>
#include "nrf_drv_clock.h"
#include "nrf_fstorage_nvmc.h"

/*---------------------------------------------------------------------------*/
PROCESS(simple_nrf52840_nvmc_flash, "Simple nvmc flash example on nrf52840");
AUTOSTART_PROCESSES(&simple_nrf52840_nvmc_flash);
/*---------------------------------------------------------------------------*/

/*
 * Define callbacks for fstorage events
 */
static void flash_callback(nrf_fstorage_evt_t *p_evt)
{
  if (p_evt->result != NRF_SUCCESS)
  {
    printf("Flash storage operation failed\n");
    return;
  }

  switch (p_evt->id)
  {
  case NRF_FSTORAGE_EVT_WRITE_RESULT:
  {
    printf("Event received: wrote %ld bytes at address 0x%lx.\n",
           p_evt->len, p_evt->addr);
  }
  break;

  case NRF_FSTORAGE_EVT_ERASE_RESULT:
  {
    printf("Event received: erased %ld page from address 0x%lx.\n",
           p_evt->len, p_evt->addr);
  }
  // unused event
  // case NRF_FSTORAGE_EVT_READ_RESULT:
  break;

  default:
    break;
  }
}

/*
 * The erase size for the chip nrf52840 is set to 4096 for nvmc
 * So we can assume a page is exactly 4096 bytes big
 * Because we will only write and update always the same
 * value, let's request a single page.
 */
NRF_FSTORAGE_DEF(nrf_fstorage_t flash_instance) =
    {
        .evt_handler = flash_callback,
        .start_addr = 0xFD000,
        .end_addr = 0xFE000,
};

/*
 * This is the address in flash were data will be written.
 */
#define FLASH_ADDR 0xFD000

static struct etimer timer;
static uint32_t write_num = 10;
static uint32_t read_num = 0;
static ret_code_t flash_ret = 0;

PROCESS_THREAD(simple_nrf52840_nvmc_flash, ev, data)
{

  PROCESS_BEGIN();

  printf("Simple nrf52840 nvmc flash example started\n");
  printf("It will write every 10 seconds a new value\n");

  // init fstorage properly
  nrf_fstorage_init(
      &flash_instance,    /* You fstorage instance, previously defined. */
      &nrf_fstorage_nvmc, /* Name of the backend. -> we use nvmc*/
      NULL                /* Optional parameter, backend-dependant. */
  );

  etimer_set(&timer, CLOCK_SECOND * 10);

  while (1)
  {
    // because we always update the same value postion in flash,
    // we need to erase it first
    // if you do not need to update the value, no erase is needed
    flash_ret = nrf_fstorage_erase(
        &flash_instance, /* The instance to use. */
        FLASH_ADDR,      /* The address to start erasing*/
        1,               /* Number of pages to erase */
        NULL             /* Optional parameter, backend-dependend.*/
    );
    if (NRF_SUCCESS == flash_ret)
    {
      printf("Erased page successfully, start writting stuff\n");

      // four bytes (uint32_t) is the smallest unit which can be written with this lib
      flash_ret = nrf_fstorage_write(
          &flash_instance,   /* The instance to use. */
          FLASH_ADDR,        /* The address in flash where to store the data. */
          &write_num,        /* A pointer to the data. */
          sizeof(write_num), /* Length of the data, in bytes. */
          NULL               /* Optional parameter, backend-dependent. */
      );
      if (NRF_SUCCESS == flash_ret)
      {
        printf("Written value %ld successfully to flash memory\n", write_num++);

        // four bytes (uint32_t) is the smallest unit which can be read with this lib
        flash_ret = nrf_fstorage_read(
            &flash_instance, /* The instance to use. */
            FLASH_ADDR,      /* The address in flash where to read data from. */
            &read_num,       /* A buffer to copy the data into. */
            sizeof(read_num) /* Length of the data, in bytes. */
        );
        if (NRF_SUCCESS == flash_ret)
        {
          printf("Successfully read value: %ld\n", read_num);
        }
        else
        {
          printf("Failed to read value from flash memory\n");
        }
      }
      else
      {
        printf("Failed to write value to flash memory\n");
      }
    }
    else
    {
      printf("Failed to erase page\n");
    }

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&timer));
    etimer_reset(&timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
