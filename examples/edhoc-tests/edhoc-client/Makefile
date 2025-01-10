CONTIKI_PROJECT = edhoc-test-client

all: $(CONTIKI_PROJECT)

CONTIKI = ../../..

MODULES += os/net/security/edhoc

#uECC fast execution optimization
CFLAGS += -DuECC_OPTIMIZATION_LEVEL=4
CFLAGS += -DuECC_SQUARE_FUNC=1

include $(CONTIKI)/Makefile.include
