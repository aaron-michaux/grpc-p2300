
#include "greeting-client.h"

#include <protos/helloworld.grpc.pb.h>

#include <grpc/support/log.h>

#include <memory>
#include <string>

namespace Greeting
{
// -- Private implementation

struct Client::Impl_
{
   using Service = helloworld::Greeter::Stub;

   explicit Impl_(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel)
       : context_{context}
       , stub_{helloworld::Greeter::NewStub(channel)}
       , stub_say_hello_{*stub_, &Service::PrepareAsyncSayHello}
   {}

   sgrpc::ExecutionContext& context_;
   std::unique_ptr<Service> stub_;
   sgrpc::RpcStub<Service, helloworld::HelloRequest, helloworld::HelloReply> stub_say_hello_;
};

// -- Construction/Destruction

Client::Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel)
    : impl_{std::make_unique<Impl_>(context, std::move(channel))}
{}

Client::~Client() = default;

// -- RPC interface

sgrpc::RpcSender<std::string> Client::say_hello(std::string user)
{
   helloworld::HelloRequest request;
   request.set_name(std::move(user));

   struct ConvertResult
   {
      std::string operator()(const helloworld::HelloReply& reply) { return reply.message(); }
   };

   return impl_->stub_say_hello_.call<std::string, ConvertResult>(impl_->context_,
                                                                  std::move(request));
}

} // namespace Greeting
