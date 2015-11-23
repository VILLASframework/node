import re

class OcbMapping:
	def __init__(self, entityId, entityType, attributeName, attributeType):
		self.entityId = entityId
		self.entityType = entityType
		self.attributeName = attributeName
		self.attributeType = attributeType
		
	@classmethod
	def parse(self, lines):
		m = re.match('([a-z]+)\(([a-z]+)\)\.([a-z]+).\(([a-z]+)\)', re.I)
		return OcbMapping(m.group(1), m.group(2), m.group(3), m.group(4))
	
	@classMethod
	def parseFile(self, file):

class OcbEntity:

class OcbAttribute:

class OcbResponse:
	def __init__(self, mapping):
		self.mapping = mapping

	def getJson(self, msg):
		json = { "contextResponses" : [ ] }
				
		for (value in msg.values):
			json["contextResponses"].append({
				"statusCode" : { "code" : 200, "reasonPhrase" : "OK" }
				"contextElement" : {
					"attributes" = [ ],
					"id" : "",
					"isPattern" : false,
					"type"
				}
			})
