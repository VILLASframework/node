# GTFPGA {#gtfpga}

The GTFPGA card is an extension card for simulator racks from RTDS.
The manufacturer provides a VHDL module to exchange data via fiber optics between the GTFPGA and GPC cards.

The GTFPGA card is based on the Xilinx ML507 or ML605 evaluation boards.
This board consists of a Virtex 5 FPGA, Ethernet and Fiberoptic MACs and an hard macro PowerPC core.

This node type uses the PCIexpress bus to communicate with the FPGA cards.

## Configuration

Every `gtfpga` node support the following special settings:

#### `slot`

#### `id`

#### `rate`

### Example

	nodes = {
		gtfpga_node = {
			type = "gtfpga",
	
		### The following settings are specific to the gtfpga node-type!! ###
	
			slot = "01:00.0",			# The PCIe slot location (see first column in 'lspci' output)
			id = "1ab8:4005",			# The PCIe vendor:device ID (see third column in 'lspci -n' output)
	
			rate = 1
		}
	}
