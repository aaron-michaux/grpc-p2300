
#pragma once

#include "stdinc.hpp"

#include "detail/base_inc.hpp"

#include "execution_context.hpp"
#include "inflight_rpc.hpp"
#include "response_reader_factory.hpp"
#include "scheduler.hpp"

namespace sgrpc {

template <typename Service, typename RequestType, typename ResponseType> class PureRpcSender {
private:
  template <typename R> struct Op_ {
    ExecutionContext& context_;
    ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
    std::shared_ptr<RequestType> request_;
    [[no_unique_address]] R receiver_;

    template <typename Message>
    void set_error(R&& receiver, grpc::StatusCode status_code, Message&& message) noexcept {
      stdexec::set_error(std::move(receiver), grpc::Status{status_code, message});
    }

    friend void tag_invoke(stdexec::start_t, Op_& self) noexcept { self.invoke(); }

    void invoke() noexcept {
      const bool invoked = context_.post(
          [this](grpc::CompletionQueue& cq) mutable -> std::unique_ptr<CompletionQueueEvent> {
            // Factory for creating the correct response writer type
            auto reader_factory = [this, &cq](grpc::ClientContext& client_context,
                                              RequestType request) mutable {
              return factory_fn_(&client_context, std::move(request), &cq);
            };

            // Sets the value on the receiver when the rpc call completes
            auto completion = [this](bool is_ok, const grpc::Status& status,
                                     const ResponseType& response) mutable {
              if (!is_ok) {
                set_error(std::move(receiver_), grpc::StatusCode::CANCELLED,
                          "operation posted after shutdown");
              } else if (status.ok()) {
                fmt::print("response: {}\n", response.message());
                try {
                  stdexec::set_value(std::move(receiver_), std::move(response));
                } catch (std::exception& e) {
                  set_error(std::move(receiver_), grpc::StatusCode::INVALID_ARGUMENT,
                            fmt::format("exception unpacking protobuf, {}", e.what()));
                } catch (...) {
                  set_error(std::move(receiver_), grpc::StatusCode::INVALID_ARGUMENT,
                            "unknown exception unpacking protobuf");
                }
              } else {
                set_error(std::move(receiver_), status.error_code(), status.error_message());
              }
            };

            return std::make_unique<InflightRpc<RequestType, ResponseType>>(
                std::move(reader_factory), std::move(*request_), std::move(completion));
          });

      if (!invoked) { // Immediately set the value to "CANCELLED"
        stdexec::set_error(std::move(receiver_),
                           grpc::Status{grpc::StatusCode::CANCELLED, "rpc was not scheduled"});
      }
    }
  };

  template <class R> friend auto tag_invoke(stdexec::connect_t, PureRpcSender self, R&& receiver) {
    return Op_<std::remove_cvref_t<R>>{self.context_, std::move(self.factory_fn_),
                                       std::move(self.request_), std::move(receiver)};
  }

  friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                              PureRpcSender self) noexcept {
    return Scheduler{self.context_};
  }

public:
  using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(ResponseType),
                                                               stdexec::set_error_t(grpc::Status)>;

  PureRpcSender(ExecutionContext& context,
                ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn,
                RequestType request)
      : context_{context},                                          //
        factory_fn_{std::move(factory_fn)},                         //
        request_{std::make_shared<RequestType>(std::move(request))} //
  {}

private:
  ExecutionContext& context_;
  ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
  std::shared_ptr<RequestType> request_;
};

} // namespace sgrpc
