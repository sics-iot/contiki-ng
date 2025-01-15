CONTIKI_PROJECT = 6lr-with-slip
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

PLATFORMS_EXCLUDE = native

MODULES += os/services/demux/mac-demux
ifeq ($(MAKE_WITH_ORCHESTRA), 1)
MODULES += os/services/orchestra
endif

CFLAGS += -I..

include $(CONTIKI)/Makefile.include
