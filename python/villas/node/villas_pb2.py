# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# NO CHECKED-IN PROTOBUF GENCODE
# source: villas.proto
# Protobuf Python Version: 5.29.3
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import runtime_version as _runtime_version
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
_runtime_version.ValidateProtobufRuntimeVersion(
    _runtime_version.Domain.PUBLIC,
    5,
    29,
    3,
    '',
    'villas.proto'
)
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\x0cvillas.proto\x12\x0bvillas.node\"/\n\x07Message\x12$\n\x07samples\x18\x01 \x03(\x0b\x32\x13.villas.node.Sample\"\xfe\x01\n\x06Sample\x12,\n\x04type\x18\x01 \x02(\x0e\x32\x18.villas.node.Sample.Type:\x04\x44\x41TA\x12\x10\n\x08sequence\x18\x02 \x01(\x04\x12)\n\tts_origin\x18\x03 \x01(\x0b\x32\x16.villas.node.Timestamp\x12+\n\x0bts_received\x18\x04 \x01(\x0b\x32\x16.villas.node.Timestamp\x12\x11\n\tnew_frame\x18\x05 \x01(\x08\x12\"\n\x06values\x18\x64 \x03(\x0b\x32\x12.villas.node.Value\"%\n\x04Type\x12\x08\n\x04\x44\x41TA\x10\x01\x12\t\n\x05START\x10\x02\x12\x08\n\x04STOP\x10\x03\"&\n\tTimestamp\x12\x0b\n\x03sec\x18\x01 \x02(\r\x12\x0c\n\x04nsec\x18\x02 \x02(\r\"Z\n\x05Value\x12\x0b\n\x01\x66\x18\x01 \x01(\x01H\x00\x12\x0b\n\x01i\x18\x02 \x01(\x03H\x00\x12\x0b\n\x01\x62\x18\x03 \x01(\x08H\x00\x12!\n\x01z\x18\x04 \x01(\x0b\x32\x14.villas.node.ComplexH\x00\x42\x07\n\x05value\"%\n\x07\x43omplex\x12\x0c\n\x04real\x18\x01 \x02(\x02\x12\x0c\n\x04imag\x18\x02 \x02(\x02')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'villas_pb2', _globals)
if not _descriptor._USE_C_DESCRIPTORS:
  DESCRIPTOR._loaded_options = None
  _globals['_MESSAGE']._serialized_start=29
  _globals['_MESSAGE']._serialized_end=76
  _globals['_SAMPLE']._serialized_start=79
  _globals['_SAMPLE']._serialized_end=333
  _globals['_SAMPLE_TYPE']._serialized_start=296
  _globals['_SAMPLE_TYPE']._serialized_end=333
  _globals['_TIMESTAMP']._serialized_start=335
  _globals['_TIMESTAMP']._serialized_end=373
  _globals['_VALUE']._serialized_start=375
  _globals['_VALUE']._serialized_end=465
  _globals['_COMPLEX']._serialized_start=467
  _globals['_COMPLEX']._serialized_end=504
# @@protoc_insertion_point(module_scope)
