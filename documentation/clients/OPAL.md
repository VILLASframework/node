# OPAL-RT {#opal}

@defgroup opal OPAL-RT

The communication between OPAL-RT models and the S2SS is established by using ansychronus programs.
Asynchronous programs are a feature of RT-LAB.

## Source Code

The source code of the asynchronus program is located at:

	/clients/opal/models/AsyncIP_sl/s2ss/

## Arguments for OPAL-RT block

| Param		 | Description   | Example Value  |
| :------------- | :------------ |:-------------- |
| FloatParam[0]	 | Protocol      |                |
| FloatParam[1]  | RemotePort    | 10200          |
| FloatParam[2]  | LocalPort	 | 10201          |
| StringParam[0] | RemoteAddr	 | 192.168.0.10   |
| StringParam[1] | LocalAddr	 | 192.168.0.11   |
| StringParam[2] | InterfaceName | eth2           |

@todo Complete documentation for the OPAL-RT Simulink module.
