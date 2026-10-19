# Pixel Dash - Nintendo Switch homebrew (.nro)
# Требуется devkitPro: devkitA64 + libnx (в GitHub Actions ставится автоматически)

ifeq ($(strip $(DEVKITPRO)),)
DEVKITPRO := /opt/devkitpro
endif

TARGET     := pixeldash
APP_TITLE  := Pixel Dash
APP_AUTHOR := You
APP_VERSION:= 1.0.0

PREFIX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
CC     := $(PREFIX)gcc
ARCH   := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE
CFLAGS := -O2 -Wall -ffunction-sections $(ARCH) -D__SWITCH__ \
          -I$(DEVKITPRO)/libnx/include -Isource
LDFLAGS:= -specs=$(DEVKITPRO)/libnx/switch.specs -g $(ARCH)
LIBS   := -L$(DEVKITPRO)/libnx/lib -lnx -lm

SRC := source/main.c source/game.c
OBJ := $(SRC:.c=.o)

all: $(TARGET).nro

%.o: %.c source/game.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET).elf: $(OBJ)
	$(CC) $(LDFLAGS) $(OBJ) $(LIBS) -o $@

$(TARGET).nacp:
	$(DEVKITPRO)/tools/bin/nacptool --create "$(APP_TITLE)" "$(APP_AUTHOR)" "$(APP_VERSION)" $@

$(TARGET).nro: $(TARGET).elf $(TARGET).nacp icon.jpg
	$(DEVKITPRO)/tools/bin/elf2nro $< $@ --icon=icon.jpg --nacp=$(TARGET).nacp

clean:
	rm -f $(OBJ) $(TARGET).elf $(TARGET).nro $(TARGET).nacp

.PHONY: all clean
