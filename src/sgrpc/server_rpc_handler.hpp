
#pragma once

#include "detail/base_inc.hpp"

#include "sgrpc/rpc_sender.hpp"
#include "sgrpc/scheduler.hpp"

#include "detail/completion_queue_event.hpp"

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
 * @brief Factory method to create `BindRpcRequestFunction` thunks.
 *
 * ~~~
 * using Service = helloworld::Greeter::AsyncService;
 * auto bind_thunk = bind_rpc(service, &Service::RequestSayHello);
 * ~~~
 */
auto bind_rpc(auto& service, auto bind_thunk)
{
   using std::placeholders::_1;
   using std::placeholders::_2;
   using std::placeholders::_3;
   using std::placeholders::_4;
   using std::placeholders::_5;
   using std::placeholders::_6;
   return std::bind(bind_thunk, std::ref(service), _1, _2, _3, _4, _5, _6);
}

/**
 * @brief Logic to execute to service a server-side RPC
 */
template<typename RequestType, typename ResponseType>
using RpcLogicThunk = std::function<ResponseType(const grpc::ServerContext& server_context,
                                                 const RequestType& request_envelope)>;

auto bind_logic(auto& server, auto logic_thunk)
{
   using std::placeholders::_1;
   using std::placeholders::_2;
   return std::bind(logic_thunk, std::ref(server), _1, _2);
}

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
         typename BindRequest = BindRpcRequestFunction<RequestType, ResponseType>,
         typename RpcLogic    = RpcLogicThunk<RequestType, ResponseType>>
class ServerRpcHandler : public CompletionQueueEvent
{
 private:
   enum class Status : int { Start, Finish };
   using LogicResultType =
       typename std::result_of<RpcLogic && (const grpc::ServerContext&, const RequestType&)>::type;

 public:
   // Scheduler: Where to put the async computation
   // Service+MemFn: The rpc service, eg.,        std::mem_fn(&Service::RequestSayHello)
   // Server+MemFn: Logic to handle request, eg., std::string say_hello(std::string)
   // Conversion Functions: Convert RPC types to/from application types
   // CompletionQueue: Used by grpc to process events

   ServerRpcHandler(Scheduler scheduler,
                    BindRequest bind_request,
                    RpcLogic logic,
                    grpc::ServerCompletionQueue& cq)
       : scheduler_{scheduler} // , service_{service}
       , bind_request_{bind_request}
       , logic_{logic}
       , cq_{cq}
       , response_writer_{&server_context_}
   {
      // Bind the request (this object) to completion queue `cq`.
      // Note: `this` lifecycle now controlled by the completion queue
      bind_request_(&server_context_, &request_, &response_writer_, &cq_, &cq_, this);
   }

   ~ServerRpcHandler() = default;

   void complete(bool is_okay) noexcept override
   {
      if(delete_on_next_complete_ || !is_okay) {
         delete this;

      } else {
         // Spawn a new RpcCallContext to service incoming requests
         new ServerRpcHandler{scheduler_, bind_request_, logic_, cq_};

         // Will delete on next call to `proceed`
         delete_on_next_complete_ = true;

         // This is "immediate-mode" logic
         constexpr bool is_executed_immediately = !stdexec::sender<LogicResultType>;
         if constexpr(is_executed_immediately) {
            try {
               response_writer_.Finish(logic_(server_context_, request_), grpc::Status::OK, this);
            } catch(...) {
               // TODO: log here
               response_writer_.Finish(
                   ResponseType{}, grpc::Status{grpc::StatusCode::INTERNAL, ""}, this);
            }

         } else {
            // Schedule the sender for execution
            stdexec::sender auto work
                = stdexec::schedule(scheduler_)       // Execute on execution_context
                  | logic_(server_context_, request_) // The specified logic
                  | stdexec::then([this](const ResponseType& response) { // Write response
                       response_writer_.Finish(response, grpc::Status::OK, this);
                    })
                  | stdexec::upon_error([this](auto... arg) { // Handle errors
                       // auto status = grpc::Status{
                       //     detail::to_grpc_status_code(status.error_code()),
                       //     std::string{std::cbegin(status.details()),
                       //     std::cend(status.details())}};
                       auto grpc_status = grpc::Status{grpc::StatusCode::UNKNOWN, "TBA"};
                       response_writer_.Finish(ResponseType{}, grpc_status, this);
                    });

            // Action!
            stdexec::start_detached(work);
         }
      }
   }

 private:
   Scheduler scheduler_;

   BindRequest bind_request_;
   RpcLogic logic_;

   grpc::ServerCompletionQueue& cq_;
   grpc::ServerContext server_context_;
   grpc::ServerAsyncResponseWriter<ResponseType> response_writer_;

   RequestType request_;

   bool delete_on_next_complete_{false};
};
} // namespace sgrpc
