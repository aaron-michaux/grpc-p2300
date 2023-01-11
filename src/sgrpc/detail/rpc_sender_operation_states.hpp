
#pragma once

#include "base_inc.hpp"
#include "inflight_rpc.hpp"
#include "response_reader_factory.hpp"
#include "utils.hpp"

#include "sgrpc/execution_context.hpp"
#include "sgrpc/rpc_status.hpp"

#include <fmt/format.h>
#include <functional>

namespace sgrpc
{
template<typename ResultType>
using WrappedCompletionHandler
    = std::function<void(bool is_ok, const grpc::Status& status, std::optional<ResultType> result)>;

template<typename ResultType>
using WrappedRpcFactory
    = std::function<RpcFactory(WrappedCompletionHandler<ResultType> completion)>;
} // namespace sgrpc

namespace sgrpc::detail
{

/**
 * Operation State for a "pure" (unwrapped) RPC Sender
 */
template<typename Service, typename RequestType, typename ResponseType, typename Receiver>
struct PureRpcSenderOpState
{
   ExecutionContext& context_;
   ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
   RequestType request_;
   [[no_unique_address]] Receiver receiver_;

   PureRpcSenderOpState(ExecutionContext& context,
                        ResponseReaderFactory<Service, RequestType, ResponseType>&& factory_fn,
                        Receiver&& receiver)
       : context_{context}
       , factory_fn_{std::move(factory_fn)}
       , receiver_{std::move(receiver)}
   {}
   PureRpcSenderOpState(PureRpcSenderOpState&&)            = delete;
   PureRpcSenderOpState& operator=(PureRpcSenderOpState&&) = delete;

   friend void tag_invoke(stdexec::start_t, PureRpcSenderOpState& self) noexcept { self.invoke(); }

   void invoke() noexcept
   {
      const bool invoked = context_.post(
          [this](grpc::CompletionQueue& cq) mutable -> std::unique_ptr<CompletionQueueEvent> {
             // Factory for creating the correct response writer type
             auto reader_factory = [this, &cq](grpc::ClientContext& client_context) mutable {
                return factory_fn_(&client_context, std::move(request_), &cq);
             };

             // Sets the value on the receiver when the rpc call completes
             auto completion = [this](bool is_ok,
                                      const grpc::Status& status,
                                      const ResponseType& response) mutable {
                if(!is_ok) {
                   stdexec::set_error(std::move(receiver_),
                                      grpc::Status{grpc::StatusCode::UNAVAILABLE,
                                                   "operation posted after shutdown"});

                } else if(!status.ok()) {
                   stdexec::set_error(std::move(receiver_),
                                      grpc::Status{status.error_code(), status.error_message()});

                } else {
                   try {
                      stdexec::set_value(std::move(receiver_), std::move(response));

                   } catch(std::exception& e) {
                      stdexec::set_error(
                          std::move(receiver_),
                          grpc::Status{grpc::StatusCode::INTERNAL,
                                       fmt::format("exception unpacking protobuf, {}", e.what())});

                   } catch(...) {
                      stdexec::set_error(std::move(receiver_),
                                         grpc::Status{grpc::StatusCode::INTERNAL,
                                                      "unknown exception unpacking protobuf"});
                   }
                }
             };

             return std::make_unique<InflightRpc<ResponseType>>(std::move(reader_factory),
                                                                std::move(completion));
          });

      if(!invoked)
         stdexec::set_error(
             std::move(receiver_),
             grpc::Status{grpc::StatusCode::UNAVAILABLE,
                          "rpc was not scheduled because the service was unavailable"});
   }
};

/**
 * The setup CallData for creating a type-erased Rpc
 */
template<typename Service,
         typename RequestType,
         typename ResponseType,
         typename ResultType,
         typename ConversionFunction>
struct CallData
{
   sgrpc::ExecutionContext& context;                                     //!< For scheduling
   ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn; //!< Prepares the rpc
   RequestType request;                                                  //!< Input to rpc
   WrappedCompletionHandler<ResultType> completion;                      //!< Put result on Receiver

   std::unique_ptr<CompletionQueueEvent> operator()(grpc::CompletionQueue& cq)
   {
      auto factory = [this, &cq](grpc::ClientContext& client_context) mutable {
         return factory_fn(&client_context, std::move(request), &cq);
      };

      auto curried_completion = [completion = std::move(completion)](bool is_ok,
                                                                     const grpc::Status& status,
                                                                     const ResponseType& response) {
         try {
            ConversionFunction convert;
            completion(is_ok, status, convert(response));
         } catch(std::exception& e) {
            completion(is_ok,
                       grpc::Status{grpc::StatusCode::INTERNAL,
                                    fmt::format("exception unpacking protobuf, {}", e.what())},
                       {});
         } catch(...) {
            completion(is_ok,
                       grpc::Status{grpc::StatusCode::INTERNAL, "exception unpacking protobuf"},
                       {});
         }
      };

      return std::make_unique<InflightRpc<ResponseType>>(std::move(factory),
                                                         std::move(curried_completion));
   }
};

/**
 * Operation State for a type-erased RPC Sender
 */
template<typename Receiver, typename ResultType> struct RpcSenderOpState
{
   static constexpr bool IsVoidResultType = std::is_same<ResultType, void>::value;

   ExecutionContext& context_;
   WrappedRpcFactory<ResultType> call_factory_;
   [[no_unique_address]] Receiver receiver_;

   RpcSenderOpState(ExecutionContext& context,
                    WrappedRpcFactory<ResultType>&& call_factory,
                    Receiver&& receiver)
       : context_{context}
       , call_factory_{std::move(call_factory)}
       , receiver_{std::move(receiver)}
   {}
   RpcSenderOpState(RpcSenderOpState&&)            = delete;
   RpcSenderOpState& operator=(RpcSenderOpState&&) = delete;

   friend void tag_invoke(stdexec::start_t, RpcSenderOpState& self) noexcept { self.invoke(); }

   void invoke() noexcept
   {
      const bool invoked = context_.post(call_factory_(
          [this](bool is_ok, const grpc::Status& status, std::optional<ResultType> result) {
             if(!is_ok) {
                stdexec::set_error(std::move(receiver_), RpcStatus{RpcStatusCode::Unavailable});

             } else if(!status.ok()) {
                stdexec::set_error(std::move(receiver_),
                                   RpcStatus{detail::to_rpc_status_code(status.error_code()),
                                             status.error_message()});

             } else if(!result.has_value() && !IsVoidResultType) {
                stdexec::set_error(std::move(receiver_),
                                   sgrpc::RpcStatus{sgrpc::RpcStatusCode::LogicError});

             } else {
                try {
                   if constexpr(IsVoidResultType)
                      stdexec::set_value(std::move(receiver_));
                   else
                      stdexec::set_value(std::move(receiver_), std::move(*result));
                } catch(std::exception& e) {
                   stdexec::set_error(
                       std::move(receiver_),
                       sgrpc::RpcStatus{sgrpc::RpcStatusCode::LogicError, status.error_message()});
                } catch(...) {
                   stdexec::set_error(std::move(receiver_),
                                      sgrpc::RpcStatus{sgrpc::RpcStatusCode::LogicError,
                                                       "exception unpacking protobuf"});
                }
             }
          }));
      if(!invoked) stdexec::set_error(std::move(receiver_), RpcStatus{RpcStatusCode::Unavailable});
   }
};
} // namespace sgrpc::detail
