
#pragma once

#include "stdinc.hpp"

#include "rpc_stub.hpp"

namespace sgrpc {

template <typename ResultType>
using WrappedCompletionHandler =
    std::function<void(bool is_ok, const grpc::Status& status, std::optional<ResultType> result)>;

template <typename ResultType>
using WrappedRpcFactory =
    std::function<RpcFactory(WrappedCompletionHandler<ResultType> completion)>;

namespace detail {
template <typename Receiver, typename ResultType> struct RpcSenderOpState {
  ExecutionContext& context_;
  WrappedRpcFactory<ResultType> call_factory_;
  static constexpr bool is_void_result_type = std::is_same<ResultType, void>::value;
  [[no_unique_address]] Receiver receiver_;

  friend void tag_invoke(stdexec::start_t, RpcSenderOpState& self) noexcept { self.invoke(); }

  void invoke() noexcept {
    const bool invoked = context_.post(call_factory_(
        [this](bool is_ok, const grpc::Status& status, std::optional<ResultType> result) {
          if (!is_ok) {
            set_error(std::move(receiver_), grpc::StatusCode::CANCELLED,
                      "operation posted after shutdown");

          } else if (!status.ok()) {
            set_error(std::move(receiver_), status.error_code(), status.error_message());

          } else if (!result.has_value()) {
            if constexpr (is_void_result_type) {
              set_error(std::move(receiver_), grpc::StatusCode::INTERNAL,
                        "successful result contained no value!");
            } else {
              stdexec::set_value(std::move(receiver_)); // Handle void
            }
          } else {
            try {
              stdexec::set_value(std::move(receiver_), std::move(*result));

            } catch (std::exception& e) {
              set_error(std::move(receiver_), grpc::StatusCode::INTERNAL,
                        fmt::format("exception moving result, {}", e.what()));

            } catch (...) {
              set_error(std::move(receiver_), grpc::StatusCode::INTERNAL,
                        "exception unpacking protobuf");
            }
          }
        }));
    if (!invoked) {
      stdexec::set_error(std::move(receiver_),
                         grpc::Status{grpc::StatusCode::CANCELLED, "rpc was not scheduled"});
    }
  }

  template <typename Message>
  void set_error(Receiver&& receiver, grpc::StatusCode status_code, Message&& message) noexcept {
    stdexec::set_error(std::move(receiver), grpc::Status{status_code, message});
  }
};
} // namespace detail

template <typename ResultType> class RpcSender {
private:
  template <class R> friend auto tag_invoke(stdexec::connect_t, RpcSender self, R&& receiver) {
    return detail::RpcSenderOpState<std::remove_cvref_t<R>, ResultType>{
        self.context_, std::move(self.call_factory_), std::move(receiver)};
  }

  friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                              RpcSender self) noexcept {
    return Scheduler{self.context_};
  }

public:
  using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(ResultType),
                                                               stdexec::set_error_t(grpc::Status)>;

  RpcSender(ExecutionContext& context, WrappedRpcFactory<ResultType> call_factory)
      : context_{context}, call_factory_{std::move(call_factory)} {}

private:
  ExecutionContext& context_;
  WrappedRpcFactory<ResultType> call_factory_;
};

/**
 * Rationale:
 * + We cannot, and should not, depend on Protobuf (or any transport) objects to be
 *   semantically rich enough for application code. As such, every rpc call needs
 *   two wrapper functions: one to convert an application type to the relevant
 *   protobuf object, and another to convert the rpc's response envelope.
 * + We would like to type-erase the Service, RequestType, and ResponseType from
 *   the rest of the application logic.
 */
template <typename Service,      // The grpc service
          typename RequestType,  // The request type (protobuf) for the rpc call
          typename ResponseType> // Response type (protobuf) for the rpc call
class WrappedRpcStub {
public:
  template <typename MemberFunctionPointer>
  WrappedRpcStub(Service& service, MemberFunctionPointer mem_fn_ptr)
      : factory_fn_{service, mem_fn_ptr} {}

  // Should return a sender... hopefully does not need access to the GrpcContext here
  template <typename ResultType,         // The unwrapped result type
            typename ConversionFunction> // Functor to convert from ResponseType => ResultType
  RpcSender<ResultType> call(sgrpc::ExecutionContext& context, RequestType request) {
    WrappedRpcFactory<ResultType> factory =
        [factory_fn = factory_fn_, request = std::move(request)](
            WrappedCompletionHandler<ResultType> completion) mutable -> RpcFactory {
      return [factory_fn = std::move(factory_fn), // prepares the rpc call
              request = std::move(request),       // argument passed to the call
              completion = std::move(completion)  // unpacks
      ](grpc::CompletionQueue& cq) mutable -> std::unique_ptr<CompletionQueueEvent> {
        auto curried_reader_factory = [factory_fn = std::move(factory_fn),
                                       &cq](grpc::ClientContext& client_context,
                                            RequestType request) mutable {
          return factory_fn(&client_context, std::move(request), &cq);
        };

        auto curried_completion =
            [completion = std::move(completion)](bool is_ok, const grpc::Status& status,
                                                 const ResponseType& response) {
              try {
                ConversionFunction convert;
                completion(is_ok, status, convert(response));
              } catch (std::exception& e) {
                completion(is_ok,
                           grpc::Status{grpc::StatusCode::INTERNAL,
                                        fmt::format("exception unpacking protobuf, {}", e.what())},
                           {});
              } catch (...) {
                completion(is_ok,
                           grpc::Status{grpc::StatusCode::INTERNAL, "exception unpacking protobuf"},
                           {});
              }
            };

        return std::make_unique<InflightRpc<RequestType, ResponseType>>(
            std::move(curried_reader_factory), std::move(request), std::move(curried_completion));
      };
    };

    return {context, std::move(factory)};
  }

private:
  ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
};

} // namespace sgrpc
