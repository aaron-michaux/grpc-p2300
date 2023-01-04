
#include "stdinc.hpp"

#include "greeting-client.h"

#include "sgrpc/rpc_call.hpp"
#include "sgrpc/call_stub.hpp"

#include <protos/helloworld.grpc.pb.h>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>

namespace Greeting {

struct Client::Impl_ {
  // using namespace helloworld;
  using Service = helloworld::Greeter::Stub;

  explicit Impl_(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel)
      : context_{context},                                      //
        stub_{helloworld::Greeter::NewStub(channel)},           //
        stub_say_hello_{*stub_, &Service::PrepareAsyncSayHello} //
  {}

  sgrpc::ExecutionContext& context_;
  std::unique_ptr<Service> stub_;
  sgrpc::RpcStub<Service, helloworld::HelloRequest, helloworld::HelloReply> stub_say_hello_;
};

Client::Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel)
    : impl_{std::make_unique<Impl_>(context, std::move(channel))} {}

Client::~Client() = default;

std::string Client::sync_say_hello(std::string_view user) {
  // Data we are sending to the server.
  helloworld::HelloRequest request;
  request.set_name(user.data());

  // Container for the data we expect from the server.
  helloworld::HelloReply reply;

  // Context for the client. It could be used to convey extra information to
  // the server and/or tweak certain RPC behaviors.
  grpc::ClientContext context;

  // The producer-consumer queue we use to communicate asynchronously with the
  // gRPC runtime.
  grpc::CompletionQueue cq;

  // Storage for the status of the RPC upon completion.
  grpc::Status status;

  std::unique_ptr<grpc::ClientAsyncResponseReader<helloworld::HelloReply>> rpc(
      impl_->stub_->AsyncSayHello(&context, request, &cq));

  // Request that, upon completion of the RPC, "reply" be updated with the
  // server's response; "status" with the indication of whether the operation
  // was successful. Tag the request with the integer 1.
  rpc->Finish(&reply, &status, reinterpret_cast<void*>(1));

  void* got_tag;
  bool ok = false;
  // Block until the next result is available in the completion queue "cq".
  // The return value of Next should always be checked. This return value
  // tells us whether there is any kind of event or the cq_ is shutting down.
  GPR_ASSERT(cq.Next(&got_tag, &ok));

  // Verify that the result from "cq" corresponds, by its tag, our previous
  // request.
  GPR_ASSERT(got_tag == reinterpret_cast<void*>(1));
  // ... and that the request was completed successfully. Note that "ok"
  // corresponds solely to the request for updates introduced by Finish().
  GPR_ASSERT(ok);

  // Act upon the status of the actual RPC.
  if (status.ok()) {
    return reply.message();
  } else {
    return "RPC failed";
  }
}

sgrpc::RpcSender<std::string> Client::say_hello(std::string user) {
  helloworld::HelloRequest request;
  request.set_name(std::move(user));

  struct ConvertResult {
    std::string operator()(const helloworld::HelloReply& reply) { return reply.message(); }
  };

  return impl_->stub_say_hello_.call<std::string, ConvertResult>(impl_->context_,
                                                                 std::move(request));
}

} // namespace Greeting
