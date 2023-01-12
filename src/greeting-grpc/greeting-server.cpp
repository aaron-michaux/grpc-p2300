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
      using std::placeholders::_1;
      using std::placeholders::_2;
      using std::placeholders::_3;
      using std::placeholders::_4;
      using std::placeholders::_5;
      using std::placeholders::_6;

      // Let each completion queue server every RPC
      new sgrpc::ServerRpcContext<HelloRequest, HelloReply>(
          scheduler,
          sgrpc::bind_rpc(service, &Service::RequestSayHello),
          sgrpc::bind_logic(server, &Server::say_hello),
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
   std::shared_ptr<sgrpc::GenericServerContainer<Service, Server>> container_;
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
       = sgrpc::GenericServerContainer<Service, Server>::make(execution_context,
                                                              server,
                                                              detail::wire_rpcs,
                                                              number_server_work_queues,
                                                              port,
                                                              std::move(credentials));
   return handle;
}

} // namespace Greeting
