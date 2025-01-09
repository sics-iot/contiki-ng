
ifndef TARGET
	TARGET=nrf52840
	BOARD=dk
	NRF52840_USE_NVMC_FLASH=1
endif

CONTIKI_PROJECT = nrf52840_nvmc_flash
all: $(CONTIKI_PROJECT)
upload: $(CONTIKI_PROJECT).upload

CONTIKI = ../../../..
include $(CONTIKI)/Makefile.include
