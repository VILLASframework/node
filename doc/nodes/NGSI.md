# OMA Next Generation Services Interface {#ngsi}

The `ngsi` node type implements an interface to FIWARE context brokers following the NGSI 10 RESTful HTTP API.

This implementation if currently limited to the `updateContext` operation.
Therefore only publishing updates is supported. Subscribtion or polling of events is planned for later versions.
It's using `libcurl` and `libjansson` to communicate with the context broker over JSON.

## Configuration

You can use the `vectorize` setting to send multiple samples in a vector.

Every `ngsi` node supports the following special settings:

#### `endpoint` *(string: URL)*

#### `entity_id` *(string)*

#### `entity_type` *(string)*

#### `ssl_verify` *(boolean)*

#### `timeout` *(float: seconds)*

#### `mapping` *(array of strings)*

Example: `AttributeName(AttributeType) MetadataName1(MetadataType1)=MetadataValue1 MetadataName2(MetadataType2)=MetadataValue2 ...`

### Example

	nodes = {
		ngsi_node = {
			type = "ngsi",
		
		### The following settings are specific to the ngsi node-type!! ###
	
			endpoint = "http://46.101.131.212:1026",# The HTTP REST API endpoint of the FIRWARE context broker
		
			entity_id = "S3_ElectricalGrid",	
			entity_type = "ElectricalGridMonitoring",
		
			timeout = 5,				# Timeout of HTTP request in seconds (default is 1)
			verify_ssl = false,			# Verification of SSL server certificates (default is true)
	
			mapping = [
				"PTotalLosses(MW)",
				"QTotalLosses(Mvar)",
				"Phase(rad) min(rad)=0 max(rad)=3.141"
			]
		}
	}

## Further reading

This standard was specified by the Open Mobile Alliance (OMA).

@see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
@see http://technical.openmobilealliance.org/Technical/Release_Program/docs/NGSI/V1_0-20120529-A/OMA-TS-NGSI_Context_Management-V1_0-20120529-A.pdf