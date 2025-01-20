CURDIR := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST)))))
CONTIKI_DIR := $(abspath $(patsubst %/,%,$(dir $(lastword $(MAKEFILE_LIST))))/../..)
BINARY_SIZE_LOGFILE = $(CURDIR)/sizes.log

ifeq ($(CI), true)
  OUT_OF_TREE_TEST ?= 1
  # Use a fixed directory for CI to improve ccache hit rate.
  OUT_OF_TREE_BASE = $(HOME)
  CLEANUP_CMD = true
else
  Q ?= @
  ifeq ($(OUT_OF_TREE_TEST), 1)
    # Create a temporary directory for out-of-tree tests.
    ifeq ($(MAKELEVEL), 0)
      OUT_OF_TREE_BASE := $(shell mktemp -d)
      CLEANUP_CMD = rm -rf $(OUT_OF_TREE_BASE)/out-of-tree-tests && rmdir $(OUT_OF_TREE_BASE)
    endif
  endif
endif

OUT_OF_TREE_EXAMPLESDIR = $(OUT_OF_TREE_BASE)/out-of-tree-tests

OUT_OF_TREE_EXAMPLES = \
hello-world/native \
hello-world/zoul \
hello-world/openmote \
hello-world/cc26x0-cc13x0 \
hello-world/simplelink:BOARD=launchpad/cc26x2r1 \
hello-world/simplelink:BOARD=sensortag/cc2650 \

# Construct DIR_TARGET based on whether EXAMPLESDIR exists.
# Put a common dummy string first since targets cannot start with "-".
DIR_TARGET = examplesdir-$(wildcard $(OUT_OF_TREE_EXAMPLESDIR))
DIR_PRESENT = examplesdir-$(OUT_OF_TREE_EXAMPLESDIR)
DIR_ABSENT = examplesdir-

ifeq ($(OUT_OF_TREE_TEST), 1)
# Copy examples/hello-world to $(OUT_OF_TREE_EXAMPLESDIR), and adjust Makefile.
preparation: | $(DIR_TARGET)
	@cp -a $(CONTIKI_DIR)/examples/hello-world $(OUT_OF_TREE_EXAMPLESDIR)
	@perl -pi -e "s|^CONTIKI =.*$$|CONTIKI = $(CONTIKI_DIR)|g" $(OUT_OF_TREE_EXAMPLESDIR)/hello-world/Makefile
	$(Q)$(MAKE) EXAMPLES="$(OUT_OF_TREE_EXAMPLES)" EXAMPLESDIR=$(OUT_OF_TREE_EXAMPLESDIR) all
	$(Q)$(CLEANUP_CMD)
	$(Q)$(MAKE) OUT_OF_TREE_TEST=0 all

$(DIR_ABSENT):
	@mkdir -p $(OUT_OF_TREE_EXAMPLESDIR)

$(DIR_PRESENT):
	@echo "Remove $(OUT_OF_TREE_EXAMPLESDIR) before running this test.\n"
	@false
endif

EXAMPLESDIR=../../examples

EXAMPLES = \
6tisch/6p-packet/zoul \
6tisch/simple-node/cc2538dk:MAKE_WITH_SECURITY=1:MAKE_WITH_ORCHESTRA=1 \
6tisch/simple-node/simplelink:DEFINES=TSCH_CONF_AUTOSELECT_TIME_SOURCE=1 \
6tisch/simple-node/nrf:BOARD=nrf52840/dk \
6tisch/simple-node/nrf:BOARD=nrf52840/dongle \
6tisch/simple-node/nrf:BOARD=nrf5340/dk/application \
6tisch/simple-node/nrf:BOARD=nrf5340/dk/network \
6tisch/simple-node/gecko:BOARD=brd4162a \
6tisch/simple-node/gecko:BOARD=brd4166a \
6tisch/sixtop/zoul \
benchmarks/rpl-req-resp/zoul \
coap/coap-example-client/zoul \
coap/coap-example-server/zoul \
dev/button-hal/gecko:BOARD=brd4162a \
dev/button-hal/gecko:BOARD=brd4166a \
dev/button-hal/gecko2:BOARD=brd4002a \
dev/gpio-hal/zoul:BOARD=orion \
dev/leds/simplelink:BOARD=srf06/cc26x0 \
dev/leds/gecko:BOARD=brd4162a \
dev/leds/gecko:BOARD=brd4166a \
dev/leds/gecko2:BOARD=brd4002a \
dev/rgb-led/zoul:BOARD=remote-reva \
dev/dht11/nrf52840:BOARD=dk \
hello-world/nrf52840:BOARD=dk \
hello-world/nrf52840:BOARD=dongle \
hello-world/openmote:BOARD=openmote-b \
hello-world/openmote:BOARD=openmote-cc2538 \
hello-world/zoul:BOARD=firefly-reva:DEFINES=ZOUL_CONF_USE_CC1200_RADIO=1 \
hello-world/zoul:BOARD=firefly-reva:DEFINES=ZOUL_CONF_USE_CC1200_RADIO=1,CC1200_CONF_802154G=1 \
hello-world/simplelink:BOARD=srf06/cc13x0 \
hello-world/simplelink:BOARD=launchpad/cc1310 \
hello-world/simplelink:BOARD=launchpad/cc1312r1 \
hello-world/simplelink:BOARD=launchpad/cc1350 \
hello-world/simplelink:BOARD=launchpad/cc1350-4 \
hello-world/simplelink:BOARD=launchpad/cc1352p-2 \
hello-world/simplelink:BOARD=launchpad/cc1352p-4 \
hello-world/simplelink:BOARD=launchpad/cc1352p1 \
hello-world/simplelink:BOARD=launchpad/cc1352r1 \
hello-world/simplelink:BOARD=launchpad/cc2650 \
hello-world/simplelink:BOARD=launchpad/cc26x2r1 \
hello-world/cc26x0-cc13x0:BOARD=srf06/cc13x0 \
hello-world/cc26x0-cc13x0:BOARD=launchpad/cc2640r2 \
hello-world/cc26x0-cc13x0:BOARD=launchpad/cc2650 \
hello-world/cc26x0-cc13x0:BOARD=launchpad/cc1310 \
hello-world/cc26x0-cc13x0:BOARD=launchpad/cc1350 \
hello-world/nrf:BOARD=nrf52840/dk \
hello-world/nrf:BOARD=nrf52840/dongle \
hello-world/nrf:BOARD=nrf5340/dk/application \
hello-world/nrf:BOARD=nrf5340/dk/network \
hello-world/nrf:BOARD=nrf52840/dk:NRF_NATIVE_USB=1 \
hello-world/nrf:BOARD=nrf52840/dongle:NRF_NATIVE_USB=1 \
hello-world/nrf:BOARD=nrf5340/dk/application:NRF_NATIVE_USB=1 \
hello-world/gecko:BOARD=brd4162a \
hello-world/gecko:BOARD=brd4166a \
hello-world/gecko2:BOARD=brd4002a \
libs/data-structures/zoul \
libs/deployment/openmote \
libs/energest/zoul \
libs/ipv6-hooks/openmote \
libs/ipv6-uipbuf/zoul \
libs/logging/zoul \
libs/logging/zoul:MAKE_MAC=MAKE_MAC_TSCH \
libs/shell/openmote \
libs/simple-energest/openmote \
libs/stack-check/cc26x0-cc13x0:BOARD=srf06/cc13x0 \
libs/stack-check/cc26x0-cc13x0:BOARD=launchpad/cc2640r2 \
libs/stack-check/cc26x0-cc13x0:BOARD=launchpad/cc2650 \
libs/stack-check/cc26x0-cc13x0:BOARD=launchpad/cc1310 \
libs/stack-check/cc26x0-cc13x0:BOARD=launchpad/cc1350 \
libs/stack-check/gecko:BOARD=brd4162a \
libs/stack-check/gecko:BOARD=brd4166a \
libs/stack-check/nrf:BOARD=nrf52840/dk \
libs/stack-check/nrf:BOARD=nrf52840/dongle \
libs/stack-check/nrf:BOARD=nrf5340/dk/application \
libs/stack-check/nrf52840:BOARD=dk \
libs/stack-check/nrf52840:BOARD=dongle \
libs/stack-check/openmote \
libs/stack-check/simplelink:BOARD=srf06/cc13x0 \
libs/stack-check/simplelink:BOARD=launchpad/cc1310 \
libs/stack-check/simplelink:BOARD=launchpad/cc1312r1 \
libs/stack-check/simplelink:BOARD=launchpad/cc1350 \
libs/stack-check/simplelink:BOARD=launchpad/cc1350-4 \
libs/stack-check/simplelink:BOARD=launchpad/cc1352p-2 \
libs/stack-check/simplelink:BOARD=launchpad/cc1352p-4 \
libs/stack-check/simplelink:BOARD=launchpad/cc1352p1 \
libs/stack-check/simplelink:BOARD=launchpad/cc1352r1 \
libs/stack-check/simplelink:BOARD=launchpad/cc2650 \
libs/stack-check/zoul \
libs/stack-check/zoul:BOARD=orion \
libs/timers/zoul \
libs/trickle-library/zoul \
libs/csprng/openmote \
lwm2m-ipso-objects/zoul:DEFINES=LWM2M_Q_MODE_CONF_ENABLED=1,LWM2M_Q_MODE_CONF_INCLUDE_DYNAMIC_ADAPTATION=1\
lwm2m-ipso-objects/nrf:BOARD=nrf52840/dongle:MAKE_WITH_DTLS=1:MAKE_COAP_DTLS_WITH_PSK=1:MAKE_COAP_DTLS_WITH_CLIENT=1:MAKE_COAP_DTLS_KEYSTORE=MAKE_COAP_DTLS_KEYSTORE_SIMPLE \
mqtt-client/simplelink:BOARD=sensortag/cc2650:DEFINES=BOARD_CONF_SENSORS_DISABLE=1,TI_SPI_CONF_ENABLE=0 \
mqtt-client/zoul:BOARD=firefly:DEFINES=MQTT_CONF_VERSION=5 \
multicast/zoul \
nullnet/zoul \
platform-specific/cc2538-common/cc2538dk \
platform-specific/cc2538-common/crypto/zoul \
platform-specific/cc2538-common/pka/zoul \
platform-specific/cc26x0-cc13x0/cc26x0-web-demo/cc26x0-cc13x0:BOARD=sensortag/cc2650 \
platform-specific/cc26x0-cc13x0/cc26x0-web-demo/cc26x0-cc13x0:BOARD=sensortag/cc1350 \
platform-specific/cc26x0-cc13x0/base-demo/cc26x0-cc13x0:BOARD=sensortag/cc2650 \
platform-specific/cc26x0-cc13x0/base-demo/simplelink:BOARD=sensortag/cc1350 \
platform-specific/cc26x0-cc13x0/base-demo/simplelink:BOARD=sensortag/cc1352r1 \
platform-specific/nrf/start-network-core/nrf:BOARD=nrf5340/dk/application \
platform-specific/nrf/start-network-core/nrf:BOARD=nrf5340/dk/network \
platform-specific/nrf/trustzone/nrf:BOARD=nrf5340/dk/application \
platform-specific/zoul/at-test/zoul \
platform-specific/zoul/orion/client/zoul:BOARD=orion \
platform-specific/zoul/orion/ip64-router/zoul:BOARD=orion \
platform-specific/zoul/rev-b/zoul:BOARD=remote-revb  \
platform-specific/zoul/rtcc/zoul \
platform-specific/zoul/zoul \
rpl-border-router/cc2538dk:MAKE_ROUTING=MAKE_ROUTING_RPL_CLASSIC \
rpl-border-router/zoul:MAKE_MAC=MAKE_MAC_TSCH \
rpl-border-router/nrf:BOARD=nrf52840/dk \
rpl-border-router/nrf:BOARD=nrf52840/dongle \
rpl-border-router/nrf:BOARD=nrf5340/dk/application \
rpl-border-router/nrf:BOARD=nrf5340/dk/network \
rpl-udp/cc2538dk \
sensniff/zoul:DEFINES=ZOUL_CONF_SUB_GHZ_SNIFFER=1 \
slip-radio/zoul \
slip-radio/nrf:BOARD=nrf52840/dk \
slip-radio/nrf:BOARD=nrf52840/dongle \
slip-radio/nrf:BOARD=nrf5340/dk/application \
slip-radio/nrf:BOARD=nrf5340/dk/network \
snmp-server/cc2538dk \
storage/antelope-shell/zoul \
storage/cfs-coffee/zoul \
storage/cfs-coffee/gecko:BOARD=brd4162a \
storage/cfs-coffee/gecko:BOARD=brd4166a \
websocket/zoul \

include ../Makefile.compile-test
