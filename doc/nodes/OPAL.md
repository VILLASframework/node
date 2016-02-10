# OPAL-RT Asynchronous Process {#opal}

The communication between OPAL-RT models and the S2SS is established by using asynchronous programs.
Asynchronous programs are are a feature of RT-LAB. They are used to exchange data between Simulink models and custom C programs.
There are two ways to exchange sample values with an OPAL-RT simulator:

1. Use our adapted version of OPAL-RT's AsyncIP example for asynchronous processes (see `clients/opal`)
   In this mode, OPAL will send sample data via UDP to S2SS. S2SS has to use the `socket` node-type.
2. Run S2SS as an asynchronous process itself. This is a highly experimental feature and implemented in the node-type `opal`.
   It requires a 32-bit version of the `s2ss-server`. Data exchange is then handled using OPAL-RT's libAsyncApi.

The following description applies only to the `opal` node-type:

## Configuration

Every `opal` node supports the following special settings:

#### `send_id` *(integer)*

#### `recv_id` *(integer)*

#### `reply` *(boolean)*

### Example

	nodes = {
		opal_node = {					# The server can be started as an Asynchronous process
			type	= "opal",			# from within an OPAL-RT model.
	
		### The following settings are specific to the opal node-type!! ###
	
			send_id	= 1,				# It's possible to have multiple send / recv Icons per model
			recv_id	= 1,				# Specify the ID here.
			reply = true
		}
	}

## Arguments for OPAL-RT block

RT-LAB already provides a block to establish simple TCP/IP communication: ???
This block is based on a more generic block used for aynchronous programs: ???

@todo Insert name of RT-LAB blocks.

| Param		 | Description   | Example Value  |
| :------------- | :------------ |:-------------- |
| FloatParam[0]	 | Protocol      |                |
| FloatParam[1]  | RemotePort    | 10200          |
| FloatParam[2]  | LocalPort	 | 10201          |
| StringParam[0] | RemoteAddr	 | 192.168.0.10   |
| StringParam[1] | LocalAddr	 | 192.168.0.11   |
| StringParam[2] | InterfaceName | eth2           |

@todo Complete documentation for the OPAL-RT Simulink module.
