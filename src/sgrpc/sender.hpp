
#pragma once

#include "execution_context.hpp"

#include "detail/base_inc.hpp"

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

  void start() {
    // This is the start of a computation chain
    context_.post([this]() { set_value(receiver_, None{}); });
  }

  friend void tag_invoke(stdexec::start_t, SchedulerOperationState&& self) noexcept {
    return self.start();
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
  detail::SchedulerOperationState<Receiver> connect(Receiver receiver) {
    return {receiver, context_};
  }

  template <typename Receiver>
  friend auto tag_invoke(stdexec::connect_t, SchedulerSender self, Receiver receiver) noexcept
      -> detail::SchedulerOperationState<Receiver> {
    return self.connect(receiver);
  }

private:
  ExecutionContext& context_;
};

} // namespace sgrpc
