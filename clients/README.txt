This directory contains code and projects to connect
various simulators and tools to the S2SS server.

Author: Steffen Vogel <steffen.vogel@rwth-aachen.de>
Date: Mid 2014 - End 2015

- ml50x_cpld
    A slightly modified configuration of the CPLD on the ML507 board.
    It is based on the reference design provided by Xilinx.
    The modification redirects the PCIe reset signal directly to the Virtex 5.

- ml507_gtfpga_pcie
    A Xilinx ISE 12.4 project to directly access RTDS registers via PCIe memory.
    This is a WIP and should allow improve the latency of by directly accessing RTDS
    registers through the S2SS server which runs on the same machine.
    
- ml507_ppc440_udp
    A Xilinx XPS 12.4 project which allows access to the RTDS registers via S2SS's
    UDP protocol over the ML507 Ethernet interface.
    This is working!
    
- ml507_ppc440_plbv46_pcie
   An unsuccessful attempt to use the RTDS PLB interface used by "ml507_gtfpga_ppc440_udp"
   with the Xilinx PLBv46_PCIe bridge to allow direct PCIe access to the RTDS registers.
   This does NOT work!
   
- opal
   Contains the implementation of an asynchronous process block for RT-LAB.
   This block allows exchanging messages with an S2SS server over UDP/TCP.
   
- rscad
   Some example RSCAD drafts to test the S2SS <-> RTDS interface.
