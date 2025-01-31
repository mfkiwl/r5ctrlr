OBJ_LOCAL = linux_main.o 
COMMON_DIR = ../common
OBJ_COMMON = platform.o rproc.o rsc_table.o
TARGET = a53app.elf
SYSROOT_PATH = /xilinxworks/ZCU102/v2024.2/r5ctrlr/r5ctrlr/sysroot/sysroots/cortexa72-cortexa53-xilinx-linux

CC = /tools/Xilinx/Vitis/2024.2/gnu/aarch64/lin/aarch64-linux/bin/aarch64-linux-gnu-gcc
CFLAGS = -O0 -g3 -Wall -I../common \
         --sysroot=$(SYSROOT_PATH) \
         -I$(SYSROOT_PATH)/usr/include
LDFLAGS = -Wall \
          --sysroot=$(SYSROOT_PATH) \
          -L$(SYSROOT_PATH)/usr/lib \
          -lm -lmetal -lopen_amp
OBJ_DIR = ./obj
_OBJ_LOC = $(patsubst %,$(OBJ_DIR)/%,$(OBJ_LOCAL))
_OBJ_COM = $(patsubst %,$(OBJ_DIR)/%,$(OBJ_COMMON))
_OBJ = $(_OBJ_LOC) $(_OBJ_COM)

all: bindir $(TARGET)
default: all

bindir:
	@mkdir -p $(OBJ_DIR)

$(_OBJ_LOC): $(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(_OBJ_COM): $(OBJ_DIR)/%.o: $(COMMON_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(_OBJ)
	$(CC) $^ $(LDFLAGS) -o $@
	# rm $^

clean:
	-rm -rf $(OBJ_DIR)
	-rm -f $(TARGET)
