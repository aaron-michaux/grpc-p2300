
#pragma once

#include <grpcpp/grpcpp.h>

#include "sgrpc/execution_context.hpp"
#include "sgrpc/rpc_sender.hpp"

namespace Greeting {

/**
 * Rpc client: pack and unpack protobuf envelops to types
 *             separate channel for errors
 *             separate channel for cancels
 *             type-erase the protobuf types, and service.
 */

class Client {
public:
  explicit Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel);
  ~Client();

  std::string sync_say_hello(std::string_view user); // Should return a "future"... a "Sender"

  sgrpc::RpcSender<std::string> say_hello(std::string user);

private:
  struct Impl_;
  std::unique_ptr<Impl_> impl_;
};

} // namespace Greeting
