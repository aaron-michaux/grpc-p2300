
#include "greeting-server.h"

#include "sgrpc/sgrpc.hpp"

#include <protos/helloworld.grpc.pb.h>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace Greeting
{

using Service = helloworld::Greeter::AsyncService;

// # -- Wiring the rpc

namespace detail
{
   using namespace helloworld;

   static void wire_rpcs(Server& server,
                         Service& service,
                         sgrpc::Scheduler scheduler,
                         grpc::ServerCompletionQueue& cq)
   {
      new sgrpc::ServerRpcHandler<HelloRequest, HelloReply>(
          scheduler,
          sgrpc::bind_rpc(service, &Service::RequestSayHello),
          sgrpc::bind_logic(server, &Server::say_hello),
          cq);
   }
} // namespace detail

// # -- Type-erase the service+server using a private implementation

struct ServerContainer::Impl
{
   uint16_t port() const { return container_->port(); }
   void stop()
   {
      container_->grpc_server().Shutdown();
      //  TODO: should return a `sender`
      container_->grpc_server().Wait();
   }
   std::shared_ptr<sgrpc::GenericServerContainer<Service, Server>> container_;
};

// # -- The server container, for building and managing a live server

ServerContainer::ServerContainer()
    : impl_{std::make_shared<Impl>()}
{}
ServerContainer::~ServerContainer() { stop(); }
uint16_t ServerContainer::port() const { return impl_->port(); }
void ServerContainer::stop() { impl_->stop(); }

// # -- Building the server

ServerContainer
ServerContainer::build(sgrpc::ExecutionContext& execution_context,
                       std::shared_ptr<Server> server,
                       uint32_t number_server_work_queues,
                       uint16_t port,
                       std::shared_ptr<grpc::ServerCredentials> credentials) noexcept(false)
{
   ServerContainer handle;
   handle.impl_->container_
       = sgrpc::GenericServerContainer<Service, Server>::make(execution_context,
                                                              server,
                                                              detail::wire_rpcs,
                                                              number_server_work_queues,
                                                              port,
                                                              std::move(credentials));
   return handle;
}

} // namespace Greeting
