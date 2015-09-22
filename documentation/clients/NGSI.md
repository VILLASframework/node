# OMA Next Generation Services Interface 10 {#ngsi}

The `ngsi` node type implements an interface to FIWARE context brokers following the NGSI 10 RESTful HTTP API.

This implementation if currently limited to the `updateContext` operation.
Therefore only publishing updates is supported. Subscribtion or polling of events is planned for later versions.
It's using `libcurl` and `libjansson` to communicate with the context broker over JSON.

## Configuration

Every `ngsi` node supports the following special settings:

#### `endpoint` *(string: URL)*

#### `ssl_verify` *(boolean)*

#### `timeout` *(float: seconds)*

#### `structure` *("flat" | "children")*

  - `flat`:
  - `children`:

#### `mapping` *(array of strings)*

Format for `structure = flat`: `"entityId(entityType).attributeName(attributeType)"`

Format for `structure = children`: `"parentId(entityType).value(attributeType)"`

### Example

@todo add example from example.conf

## Further reading

This standard was specified by the Open Mobile Alliance (OMA).

@see https://forge.fiware.org/plugins/mediawiki/wiki/fiware/index.php/FI-WARE_NGSI-10_Open_RESTful_API_Specification
@see http://technical.openmobilealliance.org/Technical/Release_Program/docs/NGSI/V1_0-20120529-A/OMA-TS-NGSI_Context_Management-V1_0-20120529-A.pdf