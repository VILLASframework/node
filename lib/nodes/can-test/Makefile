CC_PATH=/opt/Xilinx/SDK/2019.1/gnu/aarch64/lin/aarch64-linux/bin
CC_PREFIX=aarch64-linux-gnu-
CPATH=$(CC_PATH)/include
LD_LIBRARY_PATH=$(CC_PATH)/lib64:$(CC_PATH)/lib

CC=$(CC_PATH)/$(CC_PREFIX)gcc
LD=$(CC)

CC_FLAGS=
LD_FLAGS=

OBJ_FILES=main.o
MISC_FILES=create_vcan.sh
FPGA_IP=137.226.133.231

BINARY=can

.PHONY: all clean deploy

all : $(BINARY)

$(BINARY) : $(OBJ_FILES)
	$(LD) $(CC_FLAGS) -o $@ $^ $(LD_FLAGS)

%.o : %.c
	$(CC) $(CC_FLAGS) -c -o $@ $<

clean:
	rm -f $(BINARY) $(OBJ_FILES)

deploy: $(BINARY)
	scp $(BINARY) root@$(FPGA_IP):~
	scp $(MISC_FILES) root@$(FPGA_IP):~
	ssh root@$(FPGA_IP) /home/root/create_vcan.sh
	ssh root@$(FPGA_IP) /home/root/$(BINARY)
