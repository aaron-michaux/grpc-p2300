
#pragma once

#include <grpcpp/grpcpp.h>

#include "sgrpc/execution_context.hpp"

namespace Greeting {

class Client {
public:
  explicit Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel);
  ~Client();

  std::string sync_say_hello(std::string_view user); // Should return a "future"... a "Sender"
  void say_hello(std::string_view user);             // Should return a "future"... a "Sender"

private:
  struct Impl_;
  std::unique_ptr<Impl_> impl_;
};

} // namespace Greeting
