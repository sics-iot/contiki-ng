CONTIKI_PROJECT = 6lr
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

ifeq ($(MAKE_WITH_ORCHESTRA), 1)
MODULES += os/services/orchestra
endif

include $(CONTIKI)/Makefile.include
