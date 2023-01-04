
#pragma once

#include "stdinc.hpp"

#include "detail/base_inc.hpp"
#include "detail/rpc_sender_operation_states.hpp"

#include "execution_context.hpp"
#include "scheduler.hpp"

namespace sgrpc {

/**
 * A Sender for grpc RPCs that uses the raw input/output types.
 */
template <typename Service, typename RequestType, typename ResponseType> class PureRpcSender {
private:
  template <class R> friend auto tag_invoke(stdexec::connect_t, PureRpcSender self, R&& receiver) {
    return detail::PureRpcSenderOpState<Service, RequestType, ResponseType, std::remove_cvref_t<R>>{
        self.context_, std::move(self.factory_fn_), std::move(self.request_), std::move(receiver)};
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

/**
 * A type-erased RpcSender: only knows about the ResultType
 */
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

} // namespace sgrpc
