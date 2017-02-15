# Enable VILLASfpga support when libxil is available
ifndef WITHOUT_FPGA
ifeq ($(shell pkg-config libxil; echo $$?),0)
	LIB_SRCS += $(wildcard  lib/fpga/*.c)
endif
endif