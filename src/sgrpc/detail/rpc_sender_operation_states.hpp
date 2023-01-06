
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

   template<typename Message>
   void set_error(Receiver&& receiver, grpc::StatusCode status_code, Message&& message) noexcept
   {
      stdexec::set_error(std::move(receiver), grpc::Status{status_code, message});
   }

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
                   set_error(std::move(receiver_),
                             grpc::StatusCode::CANCELLED,
                             "operation posted after shutdown");
                } else if(status.ok()) {
                   fmt::print("response: {}\n", response.message());
                   try {
                      stdexec::set_value(std::move(receiver_), std::move(response));
                   } catch(std::exception& e) {
                      set_error(std::move(receiver_),
                                grpc::StatusCode::INVALID_ARGUMENT,
                                fmt::format("exception unpacking protobuf, {}", e.what()));
                   } catch(...) {
                      set_error(std::move(receiver_),
                                grpc::StatusCode::INVALID_ARGUMENT,
                                "unknown exception unpacking protobuf");
                   }
                } else {
                   set_error(std::move(receiver_), status.error_code(), status.error_message());
                }
             };

             return std::make_unique<InflightRpc<ResponseType>>(std::move(reader_factory),
                                                                std::move(completion));
          });

      if(!invoked) { // Immediately set the value to "CANCELLED"
         stdexec::set_error(std::move(receiver_),
                            grpc::Status{grpc::StatusCode::CANCELLED, "rpc was not scheduled"});
      }
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
   ExecutionContext& context_;
   WrappedRpcFactory<ResultType> call_factory_;
   static constexpr bool is_void_result_type = std::is_same<ResultType, void>::value;
   [[no_unique_address]] Receiver receiver_;

   friend void tag_invoke(stdexec::start_t, RpcSenderOpState& self) noexcept { self.invoke(); }

   void invoke() noexcept
   {
      const bool invoked = context_.post(call_factory_(
          [this](bool is_ok, const grpc::Status& status, std::optional<ResultType> result) {
             if(!is_ok) {
                set_error(std::move(receiver_), RpcStatusCode::Unavailable);

             } else if(!status.ok()) {
                set_error(std::move(receiver_),
                          detail::to_rpc_status_code(status.error_code()),
                          status.error_message());

             } else if(!result.has_value()) {
                if constexpr(is_void_result_type) {
                   set_error(std::move(receiver_), RpcStatusCode::LogicError);
                } else {
                   stdexec::set_value(std::move(receiver_)); // Handle void
                }
             } else {
                try {
                   stdexec::set_value(std::move(receiver_), std::move(*result));

                } catch(std::exception& e) {
                   set_error(std::move(receiver_),
                             RpcStatusCode::LogicError,
                             fmt::format("exception moving result, {}", e.what()));

                } catch(...) {
                   set_error(std::move(receiver_),
                             RpcStatusCode::LogicError,
                             "exception unpacking protobuf");
                }
             }
          }));
      if(!invoked) { set_error(std::move(receiver_), RpcStatusCode::Unavailable); }
   }

   void set_error(Receiver&& receiver, RpcStatusCode status_code, std::string details = {}) noexcept
   {
      stdexec::set_error(std::move(receiver), RpcStatus{status_code, std::move(details)});
   }
};
} // namespace sgrpc::detail
