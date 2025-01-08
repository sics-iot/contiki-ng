CONTIKI_PROJECT = compress ecdh ecdsa
all: $(CONTIKI_PROJECT)

CONTIKI = ../../..
include $(CONTIKI)/Makefile.identify-target

# Includes software implementation of ECC
MODULES += os/services/ecc

include $(CONTIKI)/Makefile.include
