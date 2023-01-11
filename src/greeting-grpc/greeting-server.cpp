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

#include <protos/helloworld.grpc.pb.h>

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace Greeting
{
// # -- Private Implemenation

struct Server::Impl_
{
 public:
   Impl_(sgrpc::ExecutionContext& execution_context, uint32_t number_work_queues, uint16_t port)
       : execution_context_{execution_context}
       , number_work_queues_{number_work_queues}
       , port_{port}
   {
      // if port is zero, then what?
      assert(number_work_queues_ > 0);
   }

   uint16_t start(std::shared_ptr<grpc::ServerCredentials> credentials)
   {
      // Ensure that this is done at most one time
      bool expected = false;
      if(!is_started_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
         throw std::runtime_error("attempt to start the server more than once");
      }

      // Set the listening port for this server
      std::string server_address(fmt::format("0.0.0.0:{}", port_));
      int selected_port = static_cast<int>(port_);
      builder.AddListeningPort(server_address, std::move(credentials), &selected_port);

      // The instance through which RPCs are handled
      builder.RegisterService(&service_);

      // Create the work queues
      std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> queues;
      queues.reserve(number_work_queues_);
      for(auto i = 0u; i < number_work_queues_; ++i) queues.push_back(builder.AddCompletionQueue());

      // Assemble the server.
      server_ = builder.BuildAndStart();

      // What port did we get?
      if(port_ == 0) port_ = static_cast<uint16_t>(selected_port);
      assert(port_ == static_cast<uint16_t>(selected_port));

      // Register RPC callbacks onto the server completion queues
      register_rpc_callbacks_(queues);

      // Attach work-queues to the execution context... these queues live as long as the context
      execution_context_.append_server_completion_queues(std::move(queues));

      // Retun the port in use
      return port_;
   }

 private:
   void register_rpc_callbacks_(auto& queues)
   {
      //
   }

   sgrpc::ExecutionContext& execution_context_;
   grpc::ServerBuilder builder;
   helloworld::Greeter::AsyncService service_;
   std::unique_ptr<grpc::Server> server_;
   grpc::ServerContext ctx_;
   std::atomic<bool> is_started_{false};
   uint32_t number_work_queues_{0};
   uint16_t port_{0};
};

// # -- Construction/Destruction

Server::Server(sgrpc::ExecutionContext& execution_context,
               uint32_t number_work_queues,
               uint16_t port)
    : impl_{std::make_unique<Impl_>(execution_context, number_work_queues, port)}
{}

Server::~Server() {}

// # -- Action

uint16_t Server::start(std::shared_ptr<grpc::ServerCredentials> credentials)
{
   return impl_->start(std::move(credentials));
}

// using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerCompletionQueue;
using grpc::ServerContext;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;

class ServerImpl final
{
 public:
   ~ServerImpl()
   {
      server_->Shutdown();
      // Always shutdown the completion queue after the server.
      cq_->Shutdown();
   }

   // There is no shutdown handling in this code.
   void Run()
   {
      std::string server_address("0.0.0.0:50051");

      ServerBuilder builder;
      // Listen on the given address without any authentication mechanism.
      builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
      // Register "service_" as the instance through which we'll communicate with
      // clients. In this case it corresponds to an *asynchronous* service.
      builder.RegisterService(&service_);
      // Get hold of the completion queue used for the asynchronous communication
      // with the gRPC runtime.
      cq_ = builder.AddCompletionQueue();
      // Finally assemble the server.
      server_ = builder.BuildAndStart();
      std::cout << "Server listening on " << server_address << std::endl;

      // Proceed to the server's main loop.
      HandleRpcs();
   }

 private:
   // Class encompasing the state and logic needed to serve a request.
   class CallData
   {
    public:
      // Take in the "service" instance (in this case representing an asynchronous
      // server) and the completion queue "cq" used for asynchronous communication
      // with the gRPC runtime.
      CallData(Greeter::AsyncService* service, ServerCompletionQueue* cq)
          : service_(service)
          , cq_(cq)
          , responder_(&ctx_)
          , status_(CREATE)
      {
         // Invoke the serving logic right away.
         Proceed();
      }

      void Proceed()
      {
         if(status_ == CREATE) {
            // Make this instance progress to the PROCESS state.
            status_ = PROCESS;

            // As part of the initial CREATE state, we *request* that the system
            // start processing SayHello requests. In this request, "this" acts are
            // the tag uniquely identifying the request (so that different CallData
            // instances can serve different requests concurrently), in this case
            // the memory address of this CallData instance.
            service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
         } else if(status_ == PROCESS) {
            // Spawn a new CallData instance to serve new clients while we process
            // the one for this CallData. The instance will deallocate itself as
            // part of its FINISH state.
            new CallData(service_, cq_);

            // The actual processing.
            std::string prefix("Hello ");
            reply_.set_message(prefix + request_.name());

            // And we are done! Let the gRPC runtime know we've finished, using the
            // memory address of this instance as the uniquely identifying tag for
            // the event.
            status_ = FINISH;
            responder_.Finish(
                reply_,
                grpc::Status::OK,
                // grpc::Status{grpc::StatusCode::INVALID_ARGUMENT, "it was an invalid argument"},
                this);
         } else {
            GPR_ASSERT(status_ == FINISH);
            // Once in the FINISH state, deallocate ourselves (CallData).
            delete this;
         }
      }

    private:
      // The means of communication with the gRPC runtime for an asynchronous
      // server.
      Greeter::AsyncService* service_;
      // The producer-consumer queue where for asynchronous server notifications.
      ServerCompletionQueue* cq_;
      // Context for the rpc, allowing to tweak aspects of it such as the use
      // of compression, authentication, as well as to send metadata back to the
      // client.
      ServerContext ctx_;

      // What we get from the client.
      HelloRequest request_;
      // What we send back to the client.
      HelloReply reply_;

      // The means to get back to the client.
      ServerAsyncResponseWriter<HelloReply> responder_;

      // Let's implement a tiny state machine with the following states.
      enum CallStatus { CREATE, PROCESS, FINISH };
      CallStatus status_; // The current serving state.
   };

   // This can be run in multiple threads if needed.
   void HandleRpcs()
   {
      // Spawn a new CallData instance to serve new clients.
      new CallData(&service_, cq_.get());
      void* tag; // uniquely identifies a request.
      bool ok;
      while(true) {
         // Block waiting to read the next event from the completion queue. The
         // event is uniquely identified by its tag, which in this case is the
         // memory address of a CallData instance.
         // The return value of Next should always be checked. This return value
         // tells us whether there is any kind of event or cq_ is shutting down.
         GPR_ASSERT(cq_->Next(&tag, &ok));
         GPR_ASSERT(ok);
         static_cast<CallData*>(tag)->Proceed();
      }
   }

   std::unique_ptr<ServerCompletionQueue> cq_;
   Greeter::AsyncService service_;
   std::unique_ptr<grpc::Server> server_;
};

void run_server()
{
   ServerImpl server;
   server.Run();
}

} // namespace Greeting
