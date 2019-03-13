
# Cross compiling:
AS              = $(CROSS_COMPILE)as
LD              = $(CROSS_COMPILE)ld
CC              = $(CROSS_COMPILE)gcc
CPP             = $(CC) -E
AR              = $(CROSS_COMPILE)ar
NM              = $(CROSS_COMPILE)nm
STRIP           = $(CROSS_COMPILE)strip
OBJCOPY         = $(CROSS_COMPILE)objcopy
OBJDUMP         = $(CROSS_COMPILE)objdump


BASE = $(shell pwd)
BATHOS_SRC_DIR = $(shell pwd)/../bathos-mcuio-test
BATHOS_BUILD_DIR = $(BASE)/bathos-build

EXE := bathos-build/bathos
OBJS:= user_main.o

USER_CFLAGS += -I$(BASE)

export USER_CFLAGS

-include $(BATHOS_BUILD_DIR)/.config

export ESP8266_ESSID ESP8266_PASSWD ESP8266_STATION_HOSTNAME

CROSS_COMPILE ?= $(patsubst "%",%,$(CONFIG_CROSS_COMPILE))

all: $(EXE)
	esptool.py elf2image -o bathos- bathos-build/bathos

%config:
	make -C $(BATHOS_SRC_DIR) BUILD_DIR=$(BATHOS_BUILD_DIR) EXTERNAL=y $@

$(BATHOS_BUILD_DIR)/bathos-arch.mk clean:
	make -C $(BATHOS_SRC_DIR) BUILD_DIR=$(BATHOS_BUILD_DIR) EXTERNAL=y $@

$(EXE):
	make -C $(BATHOS_SRC_DIR) BUILD_DIR=$(BATHOS_BUILD_DIR) MODE=-flash EXTERNAL=y USER_OBJS=$(foreach o,$(OBJS),$(BASE)/$(o)) USER_SRC_DIR=$(BASE)
