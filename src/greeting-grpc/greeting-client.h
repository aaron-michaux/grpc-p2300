
#pragma once

#include <grpcpp/grpcpp.h>

#include "sgrpc/execution_context.hpp"
#include "sgrpc/rpc_sender.hpp"
#include "sgrpc/wrapped_rpc_stub.hpp"

// TODO, type erase the service
#include <protos/helloworld.grpc.pb.h>

namespace Greeting {

using namespace helloworld;

/**
 * Rpc client: pack and unpack protobuf envelops to types
 *             separate channel for errors
 *             separate channel for cancels
 *             type-erase the protobuf types, and service.
 */

class Client {
public:
  using Service = Greeter::Stub;

  explicit Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel);
  ~Client();

  std::string sync_say_hello(std::string_view user); // Should return a "future"... a "Sender"

  sgrpc::PureRpcSender<Service, HelloRequest, HelloReply> say_hello(HelloRequest user);

  sgrpc::RpcSender<std::string> say_hello(std::string user);

private:
  struct Impl_;
  std::unique_ptr<Impl_> impl_;
};

} // namespace Greeting
