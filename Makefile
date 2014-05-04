TARG_A = ampcontrol_gd_a
TARG_B = ampcontrol_gd_b

GD_SRCS = ks0108.c font-ks0066-ru-08.c font-ks0066-ru-24.c font-digits-32.c

SRCS = main.c $(GD_SRCS) fft.c adc.c input.c i2c.c audio.c ds1307.c
MCU = atmega16
F_CPU = 16000000L

CS = -fexec-charset=ks0066-ru

OPTIMIZE = -Os -mcall-prologues
CFLAGS = -g -Wall -Werror -lm $(OPTIMIZE) $(CS) -mmcu=$(MCU) -DF_CPU=$(F_CPU)
LDFLAGS = -g -Wall -Werror -mmcu=$(MCU)
OBJS_A = $(SRCS:.c=_a.o)
OBJS_B = $(SRCS:.c=_b.o)

CC = avr-gcc
OBJCOPY = avr-objcopy

AVRDUDE = avrdude
AD_MCU = -p m16
#AD_PROG = -c stk500v2
#AD_PORT = -P avrdoper
#AD_PROG = -c usbasp
#AD_PORT = -P usbasp

AD_CMDLINE = $(AD_MCU) $(AD_PROG) $(AD_PORT)

all: $(TARG_A) $(TARG_B)

$(TARG_A): $(OBJS_A)
	$(CC) $(LDFLAGS) -o $@.elf  $(OBJS_A) -lm
	$(OBJCOPY) -O ihex -R .eeprom -R .nwram  $@.elf $@.hex

$(TARG_B): $(OBJS_B)
	$(CC) $(LDFLAGS) -o $@.elf  $(OBJS_B) -lm
	$(OBJCOPY) -O ihex -R .eeprom -R .nwram  $@.elf $@.hex

%_a.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%_b.o: %.c
	$(CC) $(CFLAGS) -DCS_INVERTED=1 -c -o $@ $<

clean:
	rm -f $(TARG_A).{elf,bin} $(TARG_B).{elf,bin} $(OBJS_A) $(OBJS_B)

flash_a: $(TARG_A)
	$(AVRDUDE) $(AD_CMDLINE) -V -B 1.1 -U flash:w:$(TARG_A).hex:i

flash_b: $(TARG_B)
	$(AVRDUDE) $(AD_CMDLINE) -V -B 1.1 -U flash:w:$(TARG_B).hex:i

fuse:
	$(AVRDUDE) $(AD_CMDLINE) -U lfuse:w:0xff:m -U hfuse:w:0xd1:m

eeprom_tda7439:
	$(AVRDUDE) $(AD_CMDLINE) -V -B 1.1 -U eeprom:w:eeprom_tda7439.bin:r

eeprom_tda7313:
	$(AVRDUDE) $(AD_CMDLINE) -V -B 1.1 -U eeprom:w:eeprom_tda7313.bin:r

eeprom_tda7318:
	$(AVRDUDE) $(AD_CMDLINE) -V -B 1.1 -U eeprom:w:eeprom_tda7318.bin:r
