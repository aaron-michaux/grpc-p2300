
#pragma once

#include <grpcpp/grpcpp.h>

#include "sgrpc/execution_context.hpp"
#include "sgrpc/rpc_sender.hpp"

// TODO, type erase the service
#include <protos/helloworld.grpc.pb.h>

namespace Greeting {

using namespace helloworld;

class Client {
public:
  using Service = Greeter::Stub;

  explicit Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel);
  ~Client();

  std::string sync_say_hello(std::string_view user); // Should return a "future"... a "Sender"

  sgrpc::RpcSender<Service, HelloRequest, HelloReply> say_hello(HelloRequest user);

private:
  struct Impl_;
  std::unique_ptr<Impl_> impl_;
};

} // namespace Greeting
