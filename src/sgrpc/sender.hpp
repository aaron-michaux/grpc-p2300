
#pragma once

#include "execution_context.hpp"

namespace sgrpc {

struct None {}; // Not-a-value

class ExecutionContext;

namespace detail {
// The result type of calling "connect(sender, receiver)"
template <typename Sender, typename Receiver>
using connect_result_type = decltype(connect(std::declval<Sender>(), std::declval<Receiver>()));

// Access to the Sender's result_type
template <typename Sender> using sender_result_type = typename Sender::result_type;

/**
 * The operation-state
 */
template <typename Receiver> class SchedulerOperationState final {
  //@{ Construction
  SchedulerOperationState(Receiver receiver, ExecutionContext& context)
      : receiver_{std::move(receiver)}, context_{context} {}
  SchedulerOperationState(const SchedulerOperationState&) = delete;
  SchedulerOperationState(SchedulerOperationState&&) = delete;
  ~SchedulerOperationState() = default;
  SchedulerOperationState& operator=(const SchedulerOperationState&) = delete;
  SchedulerOperationState& operator=(SchedulerOperationState&&) = delete;
  //@}

  friend void start(SchedulerOperationState& self) {
    self.context_.post([]() { set_value(receiver_, None{}); }); // start of a computation chain
  }

private:
  Receiver receiver_;
  ExecutionContext& context_;
};

} // namespace detail

class SchedulerSender final {
public:
  using result_type = None;

  //@{ construction
  constexpr SchedulerSender(ExecutionContext& context) : context_{context} {}
  //@}

  template <typename Receiver>
  friend detail::SchedulerOperationState<Receiver> connect(SchedulerSender sender,
                                                           Receiver receiver) {
    return {receiver, sender.context_};
  }

private:
  ExecutionContext& context_;
};

// template <typename Service, typename PrepareAsyncMemFun, typename RequestType,
//           typename ResponseType>
// class RpcCall {
// public:
//   void send_to_wire() {
//     response_reader_ = reader_factory_(service_, &client_context_, request_, &cq_);
//     response_reader_->StartCall();
//     response_reader_->Finish(&response_, &status, this); // scheduled
//   }

//   RequestType& request() noexcept { return request_; }
//   const RequestType& request() const noexcept { return request_; }

// private:
//   Service& service_;
//   PrepareAsyncMemFun reader_factory_;
//   grpc::ClientContext client_context_;
//   grpc::Status status_;
//   RequestType request_;
//   ResponseType response_;
//   std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> response_reader_;
// };

} // namespace sgrpc
