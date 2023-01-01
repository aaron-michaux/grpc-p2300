
#pragma once

#include "detail/base_inc.hpp"

#include "execution_context.hpp"
#include "inflight_rpc.hpp"
#include "response_reader_factory.hpp"
#include "scheduler.hpp"

namespace sgrpc {

template <typename Service, typename RequestType, typename ResponseType> class RpcSender {
private:
  template <typename R> struct Op_ {
    ExecutionContext& context_;
    ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
    std::shared_ptr<RequestType> request_;
    [[no_unique_address]] R receiver_;

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
                std::string message;
                stdexec::set_error(std::move(receiver_),
                                   grpc::Status{grpc::StatusCode::CANCELLED, message});
              } else if (status.ok()) {
                fmt::print("response: {}\n", response.message());
                stdexec::set_value(std::move(receiver_), std::move(response));
              } else {
                // stdexec::set_error(std::move(receiver_), std::move(status));
              }
            };

            return std::make_unique<InflightRpc<RequestType, ResponseType>>(
                std::move(reader_factory), std::move(*request_), std::move(completion));
          });

      if (!invoked) { // Immediately set the value to "CANCELLED"
        std::string message{"rpc was not scheduled"};
        stdexec::set_error(std::move(receiver_),
                           grpc::Status{grpc::StatusCode::CANCELLED, message});
      }
    }

    friend void tag_invoke(stdexec::start_t, Op_& self) noexcept { self.invoke(); }
  };

  template <class R> friend auto tag_invoke(stdexec::connect_t, RpcSender self, R&& receiver) {
    return Op_<std::remove_cvref_t<R>>{self.context_, std::move(self.factory_fn_),
                                       std::move(self.request_), std::move(receiver)};
  }

  friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                              RpcSender self) noexcept {
    return Scheduler{self.context_};
  }

public:
  using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(ResponseType),
                                                               stdexec::set_error_t(grpc::Status)>;

  RpcSender(ExecutionContext& context,
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
