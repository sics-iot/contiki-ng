CONTIKI_PROJECT = edhoc-test-server

all: $(CONTIKI_PROJECT)

# Include CoAP resources
MODULES_REL += ./resources

#uECC fast execution optimization
CFLAGS += -DuECC_OPTIMIZATION_LEVEL=4
CFLAGS += -DuECC_SQUARE_FUNC=1

CONTIKI = ../../..
include $(CONTIKI)/Makefile.dir-variables

MODULES += $(CONTIKI_NG_SECURITY_DIR)/edhoc
MODULES += $(CONTIKI_NG_SECURITY_DIR)/edhoc/server

include $(CONTIKI)/Makefile.include
