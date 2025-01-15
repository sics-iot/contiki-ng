CONTIKI_PROJECT = 6lbr
all: $(CONTIKI_PROJECT)
CONTIKI = ../../..

PLATFORMS_ONLY = native
MAKE_MAC = MAKE_MAC_OTHER

MODULES += os/services/demux/net-demux
MODULES += os/services/shell
MODULES += examples/rpl-border-router/webserver

CFLAGS += -I..
PROJECT_SOURCEFILES += custom-shell.c

connect-router: 6lbr.native
ifndef MAC_ADDR_OF_DOWNLINK_PEER
	@echo -n "$@ failed; "
	@echo "MAC_ADDR_OF_DOWNLINK_PEER is missing"
	@exit 1
else
	sudo rlwrap ./6lbr.native $(MAC_ADDR_OF_DOWNLINK_PEER)
endif

TCP_PORT ?= 60001
connect-router-cooja: 6lbr.native
ifndef MAC_ADDR_OF_DOWNLINK_PEER
	@echo -n "$@ failed; "
	@echo "MAC_ADDR_OF_DOWNLINK_PEER is missing"
	@exit 1
else
	sudo rlwrap ./6lbr.native -p $(TCP_PORT) $(MAC_ADDR_OF_DOWNLINK_PEER)
endif

include $(CONTIKI)/Makefile.include
