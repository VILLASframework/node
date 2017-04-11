# GTNETv2 card with SKT protocol

The GTNETv2 card can be upgraded with SKT firmware to send/receive UDP and TCP packets over Ethernet.
The card operates the data in big endian format. Related draft files to run the tests are in the "clients/rtds/GTNETv2_SKT/" directory.
The data can be exchanged with VILLASnode using two methods. These are implemented as part of the socket node:
 - Without a header: Only data values are sent without any header information. It can be used using header = "gtnet-skt:fake" in config file.
 - With a header: First 3 values are sequence, timestamp sec and timesatmp nanosec. Timestamp can be added using GTSYNC card (GPS source) in the draft file or if no timestamp is present (value set to 0), VILLASnode will add its own timestamp (NTP source) replacing the 0 value.
##Common Problems
Problems faced during setting up of GT-SKT card with GTSYNC card are:
 - The GT-SKT card was not detected in the RSCAD config manager because the GPC processor port was faulty. Shifting the GT-SKT card to another port solved the issue. Also check the fiber cable if that is faulty.
 - The GT-SKT display should display the correct processor number and the protocol version. Use the SEL button to toggle between processor and protocol display. The processor value should be of the form i.e. 3.1 (processor 3 GT port 1) and the protocol should be 15 which is GT-SKT. If the protocol is 16 there is an error. Also processor number 0.0 means the GT-SKT card can’t detect the processor.
 - Check that GTWIF Firmware version is 4.104 build 7 or higher and RSCAD version is 4.003.1 or higher.
 - In case the GT-SKT can’t detect the processor, restart the rack after repeating step 1. In case GT-SKT can’t detect a correct protocol, telnet (login: rtds, password: commcard) into the card and run the command “status” to see which protocol version the card has. If the card doesn’t display the correct protocol in the telnet but the “Firmware Upgrade” in RSCAD shows the correct version, downgrade the version in Firmware upgrade and then upgrade it to the desired version.
 - In case the draft file gives an error “Timing source not synced”, the GTSYNC card is not connected to GPS source.