CONTIKI_PROJECT = radio-test-mode

-include $(CONTIKI)/Makefile.identify-target

MODULES_REL += $(TARGET)

MAKE_MAC = MAKE_MAC_NULLMAC
MAKE_NET = MAKE_NET_NULLNET

PLATFORMS_ONLY = cc26x0-cc13x0

all: $(CONTIKI_PROJECT)

CONTIKI = ../../../..
include $(CONTIKI)/Makefile.include
