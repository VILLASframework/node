/* Transform HTTP API to gRPC
 *
 * Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <google/protobuf/dynamic_message.h>
#include <grpc/reflection/v1alpha/reflection.grpc.pb.h>
#include <grpc/reflection/v1alpha/reflection.pb.h>
#include <grpcpp/generic/generic_stub.h>
#include <grpcpp/grpcpp.h>

#include <villas/api/requests/node.hpp>
#include <villas/api/response.hpp>
#include <villas/nodes/gateway.hpp>
#include <villas/timing.hpp>

using namespace google::protobuf;

class ReflectionClient {
public:
  ReflectionClient(std::shared_ptr<grpc::Channel> channel)
      : stub_(grpc::reflection::v1alpha::ServerReflection::NewStub(channel)) {}
  google::protobuf::FileDescriptorSet *
  GetFileDescriptor(const std::string &symbol) {
    grpc::ClientContext context;
    auto stream = stub_->ServerReflectionInfo(&context);
    grpc::reflection::v1alpha::ServerReflectionRequest request;
    request.set_file_containing_symbol(symbol);
    bool streamstatus = stream->Write(request);
    if (!streamstatus)
      std::cout << "Server not allow reflection" << std::endl;

    grpc::reflection::v1alpha::ServerReflectionResponse response;
    google::protobuf::FileDescriptorSet *file_descs =
        new google::protobuf::FileDescriptorSet;
    if (stream->Read(&response)) {
      if (response.has_file_descriptor_response()) {
        for (const auto &bytes :
             response.file_descriptor_response().file_descriptor_proto()) {
          google::protobuf::FileDescriptorProto *file_desc =
              file_descs->add_file();
          file_desc->ParseFromString(bytes);
        }
      }
    }
    stream->WritesDone();
    grpc::Status status = stream->Finish();
    if (!status.ok()) {
      std::cerr << "Reflection failed: " << status.error_message() << std::endl;
    }
    return file_descs;
  };

private:
  std::unique_ptr<grpc::reflection::v1alpha::ServerReflection::Stub> stub_;
};

namespace villas {
namespace node {
namespace api {

class grpcRequest : public NodeRequest {
public:
  using NodeRequest::NodeRequest;
  // using GatewayRequest::GatewayRequest;

  Response *execute() override {
    gateway_node = dynamic_cast<GatewayNode *>(node);

    switch (method) {
    case Session::Method::GET:
      return executeGet();
    case Session::Method::PUT:
    case Session::Method::POST:
      return executePost();
    default:
      throw Error::invalidMethod(this);
    }
  }

  Response *executeGet() {
    std::string address = gateway_node->address;
    GatewayNode::ApiType api_type = gateway_node->type;
    if (api_type != GatewayNode::ApiType::gRPC)
      throw Error(HTTP_STATUS_NOT_FOUND, nullptr, "Api type unavailable");

    auto gRPC_package = matches[2];
    auto gRPC_service = matches[3];
    auto gRPC_method = matches[4];
    std::string methodFullName =
        "/" + gRPC_package + "." + gRPC_service + "/" + gRPC_method;

    // Form request
    grpc::Slice slice;
    pthread_mutex_lock(&gateway_node->write.mutex);
    slice = grpc::Slice(gateway_node->write.buf, gateway_node->write.wbytes);
    pthread_mutex_unlock(&gateway_node->write.mutex);
    grpc::ByteBuffer req_buf(&slice, 1);

    // gRPC call
    auto channel =
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    grpc::GenericStub generic_stub(channel);
    grpc::ClientContext context;
    grpc::CompletionQueue cq;
    grpc::ByteBuffer resp_buf;
    auto call =
        generic_stub.PrepareUnaryCall(&context, methodFullName, req_buf, &cq);
    grpc::Status status;
    call->StartCall();
    int tag = 1;
    call->Finish(&resp_buf, &status, &tag);
    void *got_tag;
    bool ok = false;
    cq.Next(&got_tag, &ok);

    if (!status.ok()) {
      cq.Shutdown();
      throw Error(HTTP_STATUS_NOT_FOUND, nullptr,
                  "Cannot connect to gRPC server");
    }

    // Parse response
    std::string recv_data;
    std::vector<grpc::Slice> slices;
    resp_buf.Dump(&slices);
    for (const auto &s : slices)
      recv_data.append(reinterpret_cast<const char *>(s.begin()), s.size());

    json_t *json_response;
    json_response = parse_response(resp_buf, recv_data);

    cq.Shutdown();
    return new JsonResponse(session, HTTP_STATUS_OK, json_response);
  }

  Response *executePost() {
    // Create channel & stub
    std::string address = gateway_node->address;
    GatewayNode::ApiType api_type = gateway_node->type;

    if (api_type != GatewayNode::ApiType::gRPC)
      throw Error(HTTP_STATUS_NOT_FOUND, nullptr, "Api type unavailable");

    auto gRPC_package = matches[2];
    auto gRPC_service = matches[3];
    auto gRPC_method = matches[4];
    std::string methodFullName =
        "/" + gRPC_package + "." + gRPC_service + "/" + gRPC_method;

    auto channel =
        grpc::CreateChannel(address, grpc::InsecureChannelCredentials());
    ReflectionClient refl_client(channel);

    // Form request
    // Request file descriptor from gRPC server
    grpc::ByteBuffer req_buf;
    FileDescriptorSet *protos =
        refl_client.GetFileDescriptor(gRPC_package + "." + gRPC_service);
    bool server_reflection;

    // Make descriptor database
    SimpleDescriptorDatabase db;
    for (int i = 0; i < protos->file_size(); i++) {
      db.Add(protos->file(i));
    }
    auto pool = std::make_unique<DescriptorPool>(&db);
    const Descriptor *resp_desc;
    // Check if server reflection complete
    if (protos->file_size() == 0) {
      server_reflection = false;
      req_buf = form_request();
    } else {
      server_reflection = true;
      const ServiceDescriptor *svc =
          pool->FindServiceByName(gRPC_package + "." + gRPC_service);
      if (!svc) {
        throw Error(HTTP_STATUS_NOT_FOUND, nullptr, "gRPC service not found");
      }
      const MethodDescriptor *method_desc = svc->FindMethodByName(gRPC_method);
      if (!method_desc) {
        throw Error(HTTP_STATUS_NOT_FOUND, nullptr, "gRPC method not found");
      }
      const Descriptor *req_desc = method_desc->input_type();
      resp_desc = method_desc->output_type();
      if (req_desc->full_name() == "villas.node.Message") {
        req_buf = form_request();
      } else {
        req_buf = form_request_reflection(req_desc);
      }
    }
    // gRPC all
    grpc::GenericStub generic_stub(channel);
    grpc::ClientContext context;
    grpc::CompletionQueue cq;
    grpc::ByteBuffer resp_buf;
    auto call =
        generic_stub.PrepareUnaryCall(&context, methodFullName, req_buf, &cq);
    grpc::Status status;

    call->StartCall();
    int tag = 1;
    call->Finish(&resp_buf, &status, &tag);
    void *got_tag;
    bool ok = false;
    cq.Next(&got_tag, &ok);

    if (!status.ok()) {
      cq.Shutdown();
      throw Error(HTTP_STATUS_NOT_FOUND, nullptr,
                  "Cannot connect to gRPC server");
    }

    // Parse response
    std::string recv_data;
    std::vector<grpc::Slice> slices;
    resp_buf.Dump(&slices);
    for (const auto &s : slices)
      recv_data.append(reinterpret_cast<const char *>(s.begin()), s.size());

    // Empty gRPC response
    if (recv_data.length() <= 0) {
      return new JsonResponse(session, HTTP_STATUS_OK, nullptr);
    }

    json_t *json_response;
    if (!server_reflection) {
      json_response = parse_response(resp_buf, recv_data);
    } else {
      if (resp_desc->full_name() == "villas.node.Message") {
        json_response = parse_response(resp_buf, recv_data);
      } else {
        json_response = parse_response_reflection(resp_desc, recv_data);
      }
    }
    cq.Shutdown();
    return new JsonResponse(session, HTTP_STATUS_OK, json_response);
  }

  grpc::ByteBuffer form_request_reflection(const Descriptor *req_desc) {
    grpc::Slice slice;
    DynamicMessageFactory dym_factory;
    const Message *req_prototype = dym_factory.GetPrototype(req_desc);
    std::unique_ptr<Message> request(req_prototype->New());
    // Set field
    const Reflection *refl = request->GetReflection();
    int field_count = req_desc->field_count();
    for (int i = 0; i < field_count; i++) {
      if (const FieldDescriptor *f = req_desc->field(i)) {
        json_t *json_val = json_object_get(body, f->name().data());
        if (!json_val) {
          if (f->is_required()) {
            std::cerr << "Missing required field: " << f->name() << std::endl;
          }
          continue;
        }
        if (json_val->type == json_type::JSON_ARRAY) {
          if (f->is_repeated()) {
            size_t ele_count = json_array_size(json_val);
            for (size_t j = 0; j < ele_count; j++) {
              json_t *json_array = json_array_get(json_val, j);
              switch (f->type()) {
              case grpc::protobuf::FieldDescriptor::TYPE_STRING:
                refl->AddString(request.get(), f,
                                json_string_value(json_array));
                break;
              case grpc::protobuf::FieldDescriptor::TYPE_INT32:
                refl->AddInt32(request.get(), f,
                               json_integer_value(json_array));
                break;
              case grpc::protobuf::FieldDescriptor::TYPE_INT64:
                refl->AddInt64(request.get(), f,
                               json_integer_value(json_array));
                break;
              case grpc::protobuf::FieldDescriptor::TYPE_DOUBLE:
                refl->AddDouble(request.get(), f, json_real_value(json_array));
                break;
              case grpc::protobuf::FieldDescriptor::TYPE_BOOL:
                refl->AddBool(request.get(), f, json_boolean_value(json_array));
                break;
              default:
                break;
              }
            }
          } else {
            std::cerr << "Json array in non repeat field not support"
                      << std::endl;
            continue;
          }
        } else {
          switch (f->type()) {
          case grpc::protobuf::FieldDescriptor::TYPE_STRING:
            refl->SetString(request.get(), f, json_string_value(json_val));
            break;
          case grpc::protobuf::FieldDescriptor::TYPE_INT32:
            refl->SetInt32(request.get(), f, json_integer_value(json_val));
            break;
          case grpc::protobuf::FieldDescriptor::TYPE_INT64:
            refl->SetInt64(request.get(), f, json_integer_value(json_val));
            break;
          case grpc::protobuf::FieldDescriptor::TYPE_DOUBLE:
            refl->SetDouble(request.get(), f, json_real_value(json_val));
            break;
          case grpc::protobuf::FieldDescriptor::TYPE_BOOL:
            refl->SetBool(request.get(), f, json_boolean_value(json_val));
            break;
          default:
            break;
          }
        }
      }
    }
    // Serialize
    std::string serialized;
    request->SerializeToString(&serialized);
    slice = grpc::Slice(serialized);
    grpc::ByteBuffer req_buf(&slice, 1);
    return req_buf;
  }

  grpc::ByteBuffer form_request() {
    grpc::Slice slice;
    if (body) {
      int ret;
      json_error_t err;
      double timestamp = 0;
      json_t *json_value;
      ret = json_unpack_ex(body, &err, 0, "{ s: F, s: o}", "timestamp",
                           &timestamp, "value", &json_value);
      if (ret) {
        throw Error::badRequest(nullptr, "Malformed body: {}", err.text);
      }
      if (!json_is_array(json_value)) {
        throw Error::badRequest(nullptr, "Value must be an array");
      }
      size_t i;
      json_t *json_data;
      Sample *sample_dummy = sample_alloc_mem(64);
      sample_dummy->flags =
          (int)SampleFlags::HAS_DATA | (int)SampleFlags::HAS_TS_ORIGIN;
      sample_dummy->ts.origin = time_from_double(timestamp);
      sample_dummy->length = 0;
      auto siglists = std::make_shared<SignalList>();
      json_array_foreach(json_value, i, json_data) {
        auto sig = std::make_shared<Signal>();
        switch (json_data->type) {
        case json_type::JSON_INTEGER:
          ret = sample_dummy->data[i].parseJson(SignalType::INTEGER, json_data);
          sig->type = SignalType::INTEGER;
          siglists->push_back(sig);
          break;
        case json_type::JSON_REAL:
          ret = sample_dummy->data[i].parseJson(SignalType::FLOAT, json_data);
          sig->type = SignalType::FLOAT;
          siglists->push_back(sig);
          break;
        default:
          throw Error::badRequest(nullptr, "Value type unsupported");
        }
        if (ret) {
          throw Error::badRequest(nullptr, "Value type miss match");
        }
        sample_dummy->length++;
      }
      sample_dummy->signals = siglists;
      size_t dummy_buflen = 64 * 1024;
      char *dummy_buf = new char[dummy_buflen];
      size_t dummy_wbytes;
      ret = gateway_node->formatter->sprint(dummy_buf, dummy_buflen,
                                            &dummy_wbytes, sample_dummy);
      if (ret < 0) {
        logger->warn("Failed to format request payload");
      }
      slice = grpc::Slice(dummy_buf, dummy_wbytes);
      sample_free(sample_dummy);
      delete[] dummy_buf;
    } else {
      pthread_mutex_lock(&gateway_node->write.mutex);
      slice = grpc::Slice(gateway_node->write.buf, gateway_node->write.wbytes);
      pthread_mutex_unlock(&gateway_node->write.mutex);
    }
    grpc::ByteBuffer req_buf(&slice, 1);
    return req_buf;
  }

  json_t *
  parse_response_reflection(const google::protobuf::Descriptor *resp_desc,
                            std::string &recv_data) {
    google::protobuf::DynamicMessageFactory dym_factory;
    json_t *root = json_object();
    const google::protobuf::Message *resp_prototype =
        dym_factory.GetPrototype(resp_desc);
    std::unique_ptr<google::protobuf::Message> response(resp_prototype->New());
    if (response->ParseFromString(recv_data)) {
      const google::protobuf::Reflection *resp_refl = response->GetReflection();
      int resp_field_count = resp_desc->field_count();
      for (int i = 0; i < resp_field_count; i++) {
        if (const google::protobuf::FieldDescriptor *f = resp_desc->field(i)) {
          if (f->is_repeated()) {
            auto *json_arr = json_array();
            for (int j = 0; j < resp_refl->FieldSize(*response, f); j++) {
              switch (f->type()) {
              case grpc::protobuf::FieldDescriptor::Type::TYPE_STRING:
                json_array_append(
                    json_arr,
                    json_string(
                        resp_refl->GetRepeatedString(*response, f, j).c_str()));
                break;
              case grpc::protobuf::FieldDescriptor::Type::TYPE_INT32:
                json_array_append(
                    json_arr,
                    json_integer(resp_refl->GetRepeatedInt32(*response, f, j)));
                break;
              case grpc::protobuf::FieldDescriptor::Type::TYPE_INT64:
                json_array_append(
                    json_arr,
                    json_integer(resp_refl->GetRepeatedInt64(*response, f, j)));
                break;
              case grpc::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
                json_array_append(
                    json_arr,
                    json_real(resp_refl->GetRepeatedDouble(*response, f, j)));
                break;
              case grpc::protobuf::FieldDescriptor::Type::TYPE_BOOL:
                json_array_append(
                    json_arr,
                    json_boolean(resp_refl->GetRepeatedBool(*response, f, j)));
                break;
              default:
                break;
              }
            }
            json_object_set_new(root, f->name().data(), json_arr);
          } else {
            switch (f->type()) {
            case grpc::protobuf::FieldDescriptor::Type::TYPE_STRING:
              json_object_set_new(
                  root, f->name().data(),
                  json_string(resp_refl->GetString(*response, f).c_str()));
              break;
            case grpc::protobuf::FieldDescriptor::Type::TYPE_INT32:
              json_object_set_new(
                  root, f->name().data(),
                  json_integer(resp_refl->GetInt32(*response, f)));
              break;
            case grpc::protobuf::FieldDescriptor::Type::TYPE_INT64:
              json_object_set_new(
                  root, f->name().data(),
                  json_integer(resp_refl->GetInt64(*response, f)));
              break;
            case grpc::protobuf::FieldDescriptor::Type::TYPE_DOUBLE:
              json_object_set_new(
                  root, f->name().data(),
                  json_real(resp_refl->GetDouble(*response, f)));
              break;
            case grpc::protobuf::FieldDescriptor::Type::TYPE_BOOL:
              json_object_set_new(
                  root, f->name().data(),
                  json_boolean(resp_refl->GetBool(*response, f)));
              break;
            default:
              break;
            }
          }
        }
      }
    }
    return root;
  }

  json_t *parse_response(grpc::ByteBuffer &resp_buf, std::string &recv_data) {
    // auto *json_sigs = json_array();
    json_t *json_sigs;
    pthread_mutex_lock(&gateway_node->read.mutex);
    // json_t *json_ch;
    auto *json_ch = json_array();
    json_error_t err;
    size_t rbytes;

    Sample *sample_dummy = sample_alloc_mem(64);
    int ret = gateway_node->formatter->sscan(
        recv_data.c_str(), resp_buf.Length(), &rbytes, sample_dummy);
    if (ret < 0) {
      std::cerr << "Formatting Failed: " << ret << std::endl;
    }
    for (unsigned int i = 0; i < sample_dummy->length; i++) {
      json_array_append(json_ch,
                        sample_dummy->data[i].toJson(SignalType::FLOAT));
    }
    json_sigs = json_pack_ex(&err, 0, "{s: f, s: o}", "timestamp",
                             time_to_double(&sample_dummy->ts.origin), "value",
                             json_ch);

    if (method == Session::PUT) {
      sample_copy(gateway_node->read.sample, sample_dummy);
      pthread_cond_signal(&gateway_node->read.cv);
    }

    sample_free(sample_dummy);
    pthread_mutex_unlock(&gateway_node->read.mutex);

    return json_sigs;
  }

protected:
  GatewayNode *gateway_node;
};

// Register API requests
static char n[] = "gateway";
static char r[] = "/gateway/(" RE_NODE_NAME
                  ")/([A-Za-z0-9_-]+)/([A-Za-z0-9_-]+)/([A-Za-z0-9_-]+)";
static char d[] = "gRPC api";
static RequestPlugin<grpcRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
