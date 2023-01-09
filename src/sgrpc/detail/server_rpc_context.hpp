
#pragma once

#include "sgrpc/execution_context.hpp"

#include "completion_queue_event.hpp"

#include <grpcpp/grpcpp.h>

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
template<typename RequestType, typename ResponseType>
using RpcLogicThunk
    = std::function<RpcSender<ResponseType>(const grpc::ServerContext& server_context,
                                            const RequestType& request_envelope)>;

/**
 * @brief Context for handling servier-side RPC requests
 */
template<typename RequestType,
         typename ResponseType,
         typename BindRequest = BindRpcRequestFunction<RequestType, ResponseType>,
         typename RpcLogic    = RpcLogicThunk<RequestType, ResponseType>>
class ServerRpcContext : public CompletionQueueEvent
{
 private:
   enum class Status : int { Start, Finish };

 public:
   ServerRpcContext(BindRequest bind_request, RpcLogic logic, grpc::ServerCompletionQueue& cq)
       : bind_request_{bind_request}
       , logic_{logic}
       , cq_{cq}
       , response_writer_{&server_context_}
   {
      // Bind the request to the passed completion queue
      bind_request_(&server_context_, &request_, &response_writer_, &cq_, &cq_, this);
   }

   ~ServerRpcContext() = default;

   void complete(bool is_okay) override
   {
      if(is_okay && !delete_on_next_complete_) {
         // `this` is now dedicated to
         // Spawn a new RpcCallContext to service incoming requests
         new ServerRpcContext(bind_request_, logic_, cq_);

         // Will delete on next call to `proceed`
         delete_on_next_complete_ = true;

         // Spawn the service logic
         auto sender = logic_(server_context_, request_, response_);
         then(sender,
              [this](grpc::Status status) { response_writer_.Finish(response_, status, this); });
      } else {
         delete this;
      }
   }

 private:
   BindRequest bind_request_;
   RpcLogic logic_;

   grpc::ServerCompletionQueue& cq_;
   grpc::ServerContext server_context_;
   grpc::ServerAsyncResponseWriter<ResponseType> response_writer_;

   RequestType request_;
   ResponseType response_;

   bool delete_on_next_complete_{false};
};
} // namespace sgrpc
