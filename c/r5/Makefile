OBJ_LOCAL = r5_main.o max7301.o ad3552.o adaq23876.o
COMMON_DIR = ../common
OBJ_COMMON = platform.o rproc.o rsc_table.o sample_shmem.o
TARGET = R5app.elf
PLATFORM_PATH = /xilinxworks/ZCU102/v2024.2/r5ctrlr/r5ctrlr/hardware/vitis-workspace/ZCU102-r5ctrlr

CC = /tools/Xilinx/Vitis/2024.2/gnu/armr5/lin/gcc-arm-none-eabi/bin/armr5-none-eabi-gcc
CFLAGS = -O0 -g3 -Wall -I../common \
         -I$(PLATFORM_PATH)/export/ZCU102-r5ctrlr/sw/standalone_psu_cortexr5_0/include \
         -isystem $(PLATFORM_PATH)/export/ZCU102-r5ctrlr/sw/standalone_psu_cortexr5_0/include \
         -isystem /tools/Xilinx/Vitis/2024.2/gnu/armr5/lin/gcc-arm-none-eabi/x86_64-oesdk-linux/usr/lib/arm-xilinx-eabi/gcc/arm-xilinx-eabi/13.3.0/include \
         -isystem /tools/Xilinx/Vitis/2024.2/gnu/armr5/lin/gcc-arm-none-eabi/x86_64-oesdk-linux/usr/lib/arm-xilinx-eabi/gcc/arm-xilinx-eabi/13.3.0/include-fixed \
         -isystem /tools/Xilinx/Vitis/2024.2/gnu/armr5/lin/gcc-arm-none-eabi/armrm-xilinx-eabi/usr/include \
         -DXLNX_PLATFORM -D__BAREMETAL__ -DSDT -mcpu=cortex-r5 -mfloat-abi=hard -mfpu=vfpv3-d16 -DARMR5
LDFLAGS = -Wall \
          -DSDT -mcpu=cortex-r5 -DARMR5 -mfloat-abi=hard -mfpu=vfpv3-d16 \
          -L/xilinxworks/ZCU102/v2024.2/r5ctrlr/r5ctrlr/hardware/vitis-workspace/ZCU102-r5ctrlr/export/ZCU102-r5ctrlr/sw/standalone_psu_cortexr5_0/lib \
          -Wl,-T -Wl,"lscript.ld" \
          -Wl,--start-group,-lxilstandalone  -lxiltimer  -lmetal  -lopen_amp  -lm  -lxil  -lgcc  -lc  -lm  -Wl,--end-group \
          -lxil  -lgcc  -lc \
          -lm
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
