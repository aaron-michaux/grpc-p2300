
#pragma once

#include "sgrpc/rpc_sender.hpp"
#include "sgrpc/scheduler.hpp"

#include "detail/completion_queue_event.hpp"

#include <grpcpp/grpcpp.h>

#include <functional>

namespace sgrpc
{
/**
 * @brief Function type to spawn a server-side async RPC handler
 *
 * The handler function may look like:
 * ~~~
 * service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
 * ~~~
 *
 * In which case, this member can be constructed as follows:
 * std::mem_fn(&GreetingService::RequestSayHello);
 */
template<typename RequestType, typename ResponseType>
using BindRpcRequestFunction = std::function<void(grpc::ServerContext*,
                                                  RequestType*,
                                                  grpc::ServerAsyncResponseWriter<ResponseType>*,
                                                  grpc::ServerCompletionQueue*,
                                                  grpc::ServerCompletionQueue*,
                                                  void*)>;

/**
 * @brief Logic to execute to service a server-side RPC
 */
// template<typename RequestType, typename ResponseType>
// using RpcLogicThunk
//     = std::function<RpcSender<ResponseType>(const grpc::ServerContext& server_context,
//                                             const RequestType& request_envelope)>;

template<typename RequestType, typename ResponseType>
using RpcLogicThunk = std::function<ResponseType(const grpc::ServerContext& server_context,
                                                 const RequestType& request_envelope)>;

/**
 * @brief Context for handling servier-side RPC requests
 *
 * Something like:
 * ~~~
 * new ServerRpcContext(std::mem_fn(&GreetingService::RequestSayHello),
 *                      [] (const grpc::ServerContext& context, const RequestType& request)
 *                         -> RpcSender<RequestType>
 *                      {
 *                          ...
 *                      },
 *                      server_completion_queue);
 * ~~~
 */
template<typename RequestType,
         typename ResponseType,
         typename Service,
         typename BindRequest, // = BindRpcRequestFunction<RequestType, ResponseType>,
         typename RpcLogic = RpcLogicThunk<RequestType, ResponseType>>
class ServerRpcContext : public CompletionQueueEvent
{
 private:
   enum class Status : int { Start, Finish };

 public:
   // Scheduler: Where to put the async computation
   // Service+MemFn: The rpc service, eg.,        std::mem_fn(&Service::RequestSayHello)
   // Server+MemFn: Logic to handle request, eg., std::string say_hello(std::string)
   // Conversion Functions: Convert RPC types to/from application types
   // CompletionQueue: Used by grpc to process events

   ServerRpcContext(Scheduler scheduler,
                    Service& service, // that the request is bound to
                    BindRequest bind_request,
                    RpcLogic logic,
                    grpc::ServerCompletionQueue& cq)
       : scheduler_{scheduler}
       , service_{service}
       , bind_request_{bind_request}
       , logic_{logic}
       , cq_{cq}
       , response_writer_{&server_context_}
   {
      // Bind the request to the passed completion queue
      // Note: `this`'s lifecycle now controlled by the completion queue
      bind_request_(service_, &server_context_, &request_, &response_writer_, &cq_, &cq_, this);
   }

   ~ServerRpcContext() = default;

   void complete(bool is_okay) override
   {
      if(delete_on_next_complete_ || !is_okay) {
         delete this;
      } else {
         // Spawn a new RpcCallContext to service incoming requests
         new ServerRpcContext{scheduler_, service_, bind_request_, logic_, cq_};

         // Will delete on next call to `proceed`
         delete_on_next_complete_ = true;

         response_writer_.Finish(logic_(server_context_, request_), grpc::Status::OK, this);

         // We need "immediate" (non-block) and "scheduled" (async)
         // RpcLogic

         // Spawn the service logic
         // stdexec::sender auto sender
         //     = stdexec::schedule(scheduler_)       // Execute on execution_context
         //       | logic_(server_context_, request_) // The specified logic
         //       | stdexec::then([this](const ResponseType& response) { // Write response
         //            response_writer_.Finish(response, grpc::Status::OK, this);
         //         })
         // | stdexec::upon_error([this](const RpcStatus& status) {
         //      response_writer_.Finish(
         //          ResponseType{},
         //          grpc::Status{detail::to_grpc_status_code(status.error_coode()),
         //                       std::string{std::cbegin(status.details()),
         //                                   std::cend(status.details())}},
         //          this);
         //   }) //
         //;
         // stdexec::start_detached(sender);
      }
   }

 private:
   Scheduler scheduler_;

   Service& service_; // that the request is bound to
   BindRequest bind_request_;
   RpcLogic logic_;

   grpc::ServerCompletionQueue& cq_;
   grpc::ServerContext server_context_;
   grpc::ServerAsyncResponseWriter<ResponseType> response_writer_;

   RequestType request_;

   bool delete_on_next_complete_{false};
};
} // namespace sgrpc
