/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

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

// # -- Wiring the rpc

namespace detail
{
   using namespace helloworld;
   using Service = Greeter::AsyncService;

   static void wire_rpcs(Server& server,
                         Service& service,
                         sgrpc::Scheduler scheduler,
                         grpc::ServerCompletionQueue& cq)
   {
      using std::placeholders::_1;
      using std::placeholders::_2;

      // Let each completion queue server every RPC
      new sgrpc::ServerRpcContext<HelloRequest,
                                  HelloReply,
                                  Service,
                                  decltype(std::mem_fn(&Service::RequestSayHello))>(
          scheduler,
          service,
          std::mem_fn(&Service::RequestSayHello),
          std::bind(&Server::say_hello, std::ref(server), _1, _2),
          cq);
   }
} // namespace detail

// # -- The server container

struct ServerContainer::Impl
{
   uint16_t port() const { return container_->port(); }
   void stop()
   {
      container_->grpc_server().Shutdown();
      // TODO: should return a `sender`
      container_->grpc_server().Wait();
   }
   std::shared_ptr<sgrpc::GenericServerContainer<detail::Service, Server>> container_;
};

ServerContainer::ServerContainer()
    : impl_{std::make_shared<Impl>()}
{}
ServerContainer::~ServerContainer() { stop(); }
uint16_t ServerContainer::port() const { return impl_->port(); }
void ServerContainer::stop() { impl_->stop(); }

// # -- Build the server-handle

ServerContainer
ServerContainer::build(sgrpc::ExecutionContext& execution_context,
                       std::shared_ptr<Server> server,
                       uint32_t number_server_work_queues,
                       uint16_t port,
                       std::shared_ptr<grpc::ServerCredentials> credentials) noexcept(false)
{
   ServerContainer handle;
   handle.impl_->container_
       = sgrpc::GenericServerContainer<detail::Service, Server>::make(execution_context,
                                                                      server,
                                                                      detail::wire_rpcs,
                                                                      number_server_work_queues,
                                                                      port,
                                                                      std::move(credentials));
   return handle;
}

} // namespace Greeting
