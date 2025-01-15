NVMC Flash Example
===================
This examples shows how you can use the flash memory from the nrf52840 chip.
The chip allows to use the flash memory either via the softdevice or via nvmc (non volatile memory controller).
We have focused on the second approach, hence this example does not use the softdevice at all.

It will initialize the flash memory modules and write with an successive read an increasing counter at address 0xFD000.
There will be an 10 second break between each read/write cycle.

The example requires one DK and it doesn't use SoftDevice. To compile and flash the
example run:

```bash
# compile
make TARGET=nrf52840 BOARD=dk NRF52840_USE_NVMC_FLASH=1 all
# flash
make TARGET=nrf52840 BOARD=dk NRF52840_USE_NVMC_FLASH=1 all
```

The makefile will source the properties TARGET,BOARD and NRF52840_USE_NVMC_FLASH automatically, so feel free also to use the shortcut:
```bash
make all
make upload
```
