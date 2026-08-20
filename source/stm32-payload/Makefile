ifeq ($(TARGET),stm32f427)
  CPU        = cortex-m4
  FLASH_KB   = 1024
else ifeq ($(TARGET),stm32f205)
  CPU        = cortex-m3
  FLASH_KB   = 1024
  EXTRA_CFLAGS = -DAUTOBAUD
else
  $(error Unknown TARGET=$(TARGET). Use stm32f427 or stm32f205)
endif

ASFLAGS = -mthumb -mcpu=$(CPU) -g3
CFLAGS  = -mthumb -mcpu=$(CPU) -Os -g3 -DFLASH_SIZE_KB=$(FLASH_KB) $(EXTRA_CFLAGS)
LDFLAGS = -nostartfiles -nostdlib -Tram.ld -mcpu=$(CPU) -mthumb -g3 -Wl,--no-warn-rwx-segments

all:
	arm-none-eabi-gcc -o start.o -c start.S $(ASFLAGS)
	arm-none-eabi-gcc -o main.o -c main.c $(CFLAGS)
	arm-none-eabi-gcc -o sram.elf ./start.o ./main.o $(LDFLAGS)
	arm-none-eabi-objcopy -O binary sram.elf payload.bin

clean:
	-rm -fR *.o *.elf
