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

#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace Greeting
{

namespace detail
{
   using namespace helloworld;
   using Service = Greeter::AsyncService;

   static void wire_rpcs(Server& server,
                         Service& service,
                         sgrpc::Scheduler scheduler,
                         grpc::ServerCompletionQueue& cq)
   {
      auto logic = [&server](const grpc::ServerContext& server_context,
                             const HelloRequest& request) mutable -> HelloReply {
         return server.say_hello(server_context, request);
      };

      // Let each completion queue server every RPC
      new sgrpc::ServerRpcContext<HelloRequest,
                                  HelloReply,
                                  Service,
                                  decltype(std::mem_fn(&Service::RequestSayHello)),
                                  decltype(logic)>(
          scheduler, service, std::mem_fn(&Service::RequestSayHello), logic, cq);
   }
} // namespace detail

ServerHandle
ServerHandle::build(sgrpc::ExecutionContext& execution_context,
                    std::shared_ptr<Server> server,
                    uint32_t number_server_work_queues,
                    uint16_t port,
                    std::shared_ptr<grpc::ServerCredentials> credentials) noexcept(false)
{
   auto container = sgrpc::ServerContainer<detail::Service, Server>::make(execution_context,
                                                                          server,
                                                                          detail::wire_rpcs,
                                                                          number_server_work_queues,
                                                                          port,
                                                                          std::move(credentials));

   ServerHandle handle;
   handle.server_ = server;
   handle.port_   = container->port();

   return handle;
}

// template<typename ResponseType> using RpcSender = sgrpc::ClientRpcSender<ResponseType>;

// class ActualServerInterface /* temporary name */
// {
//  public:
//    virtual ~ActualServerInterface() = default;

//    virtual void wire_rpcs(helloworld::Greeter::AsyncService& service,
//                           sgrpc::Scheduler scheduler,
//                           std::vector<std::unique_ptr<grpc::ServerCompletionQueue>>& cqs)
//        = 0;
// };

// class ActualServer /* temporary name */ : public ActualServerInterface
// {
//  public:
//    using Service = helloworld::Greeter::AsyncService;

//    void wire_rpcs(Service& service,
//                   sgrpc::Scheduler scheduler,
//                   std::vector<std::unique_ptr<grpc::ServerCompletionQueue>>& cqs) override
//    {
//       execution_context_ = &scheduler.context();

//       auto logic
//           = [this](const grpc::ServerContext& server_context,
//                    const helloworld::HelloRequest& request) mutable -> helloworld::HelloReply {
//          return say_hello(server_context, request);
//       };

//       // Let each completion queue server every RPC
//       for(auto& cq : cqs) {
//          new sgrpc::ServerRpcContext<helloworld::HelloRequest,
//                                      helloworld::HelloReply,
//                                      Service,
//                                      decltype(std::mem_fn(&Service::RequestSayHello)),
//                                      decltype(logic)>(
//              scheduler, service, std::mem_fn(&Service::RequestSayHello), logic, *cq);
//       }
//    }

//    helloworld::HelloReply say_hello(const grpc::ServerContext& server_context,
//                                     const helloworld::HelloRequest& request)
//    {
//       helloworld::HelloReply reply;
//       reply.set_message(fmt::format("Say, request='{}'", request.name()));
//       return reply;
//    }

//  private:
//    sgrpc::ExecutionContext* execution_context_{nullptr};
// };

// // # -- Private Implemenation -- most of this code belongs in the library

// class Server::Impl : public sgrpc::ServerContainerInterface,
//                      public std::enable_shared_from_this<Impl>
// {
//  public:
//    Impl(sgrpc::ExecutionContext& execution_context)
//        : execution_context_{execution_context}
//    {}

//    static std::shared_ptr<Impl> make(sgrpc::ExecutionContext& execution_context,
//                                      uint32_t number_work_queues,
//                                      uint16_t port,
//                                      std::shared_ptr<grpc::ServerCredentials> credentials)
//    {
//       auto impl = std::make_shared<Impl>(execution_context);
//       if(!impl->init(number_work_queues, port, std::move(credentials))) { return nullptr; }
//       return impl;
//    }

//    uint16_t get_port() const { return port_; }

//    std::vector<std::unique_ptr<grpc::ServerCompletionQueue>>& get_work_queues() override
//    {
//       return cqs_;
//    }

//  private:
//    bool init(uint32_t number_work_queues,
//              uint16_t port,
//              std::shared_ptr<grpc::ServerCredentials> credentials)
//    {
//       // if port is zero, then what?
//       if(number_work_queues == 0) throw std::invalid_argument{"requires at least 1 work queue"};

//       // Set the listening port for this server
//       std::string server_address(fmt::format("0.0.0.0:{}", port));
//       int selected_port = static_cast<int>(port);
//       builder.AddListeningPort(server_address, std::move(credentials), &selected_port);

//       // The instance through which RPCs are handled
//       builder.RegisterService(&service_);

//       // Create the work queues
//       cqs_.reserve(number_work_queues);
//       for(auto i = 0u; i < number_work_queues; ++i) cqs_.push_back(builder.AddCompletionQueue());

//       // Assemble the server.
//       grpc_server_ = builder.BuildAndStart();

//       // What port did we get?
//       if(port_ == 0) port_ = static_cast<uint16_t>(selected_port);
//       assert(port_ == static_cast<uint16_t>(selected_port));

//       // Register RPC callbacks onto the server completion queues
//       server_ = std::make_unique<ActualServer>();
//       server_->wire_rpcs(service_, sgrpc::Scheduler{execution_context_}, cqs_);

//       // Attach work-queues to the execution context... these queues live as long as the context
//       execution_context_.attach_server(shared_from_this());

//       // Retun the port in use
//       return true;
//    }

//    grpc::ServerBuilder builder; // Can this move into init()? I Think so
//    grpc::ServerContext ctx_;    // I think this can go

//    using Service = helloworld::Greeter::AsyncService;

//    sgrpc::ExecutionContext& execution_context_;
//    Service service_;
//    std::unique_ptr<grpc::Server> grpc_server_;
//    std::unique_ptr<ActualServer> server_;
//    std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> cqs_;
//    uint16_t port_{0};
// };

// // # -- Construction/Destruction

// Server::Server() {}
// Server::~Server() {}

// Server Server::make(sgrpc::ExecutionContext& execution_context,
//                     uint32_t number_server_work_queues,
//                     uint16_t port,
//                     std::shared_ptr<grpc::ServerCredentials> credentials) noexcept(false)
// {
//    Server server;
//    server.impl_
//        = Impl::make(execution_context, number_server_work_queues, port, std::move(credentials));
//    return server;
// }

// // # -- Getters

// uint16_t Server::get_port() const { return impl_->get_port(); }

} // namespace Greeting
