# GTFPGA {#gtfpga}

The GTFPGA card is an extension card for simulator racks from RTDS.
The manufacturer provides a VHDL module to exchange data via fiber optics between the GTFPGA and GPC cards.

The GTFPGA card is based on the Xilinx ML507 evaluation board.
This board consists of a Virtex 5 FPGA, Ethernet and Fiberoptic MACs and an integrated PowerPC core.

The PowerPC core is used to forward values between RTDS and the S2SS server.

## Configuration

Every `gtfpga` node support the following special settings:

#### `slot`

#### `id`

#### `rate`

### Example

	gtfpga_node = {
		type = "gtfpga",
		
	### The following settings are specific to the gtfpga node-type!! ###

		slot = "01:00.0",			# The PCIe slot location (see first column in 'lspci' output)
		id = "1ab8:4005",			# The PCIe vendor:device ID (see third column in 'lspci -n' output)
		
		rate = 1
	}
