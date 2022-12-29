
#include "execution_context.hpp"

#include <chrono>

namespace sgrpc {

ExecutionContext::ExecutionContext(ClientTag)
    : cq_{std::make_unique<grpc::CompletionQueue>()}, is_server_{false} {}

ExecutionContext::ExecutionContext(ServerTag)
    : cq_{std::make_unique<grpc::ServerCompletionQueue>()}, is_server_{true} {}

grpc::ServerCompletionQueue* ExecutionContext::get_server_cq() const noexcept {
  return (is_server_) ? static_cast<grpc::ServerCompletionQueue*>(cq_.get()) : nullptr;
}

bool ExecutionContext::is_running_in_thread(std::thread::id id) const noexcept {
  return thread_id_.has_value() && *thread_id_ == id;
}

/**
 * Runs until stopped
 */
bool ExecutionContext::run() {
  return run_while([]() { return false; });
}

/**
 * Runs until stopped or predicate returns `true`
 */
bool ExecutionContext::run_while(std::function<bool()> predicate) {
  if (!set_state(ExecutionState::Running))
    return false;

  void* tag = nullptr;
  bool is_ok = false;

  while (!predicate()) {
    auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds{5};
    auto status = cq_->AsyncNext(&tag, &is_ok, deadline);
    if (status == grpc::CompletionQueue::NextStatus::SHUTDOWN) {
      // cq is shutdown and fully drained
      break;

    } else if (status == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
      static_cast<GrpcOperation*>(tag)->complete(*this, is_ok);
    }
  }

  set_state(ExecutionState::Stopped);
  // Now the notify list
}

/**
 * Stops execution
 */
void ExecutionContext::stop() {
  if (get_state() == ExecutionState::Running) {
    cq_->Shutdown();
  }
}

bool set_state(ExecutionState state) {
  ExecutionState expected;
  switch (state) {
  case ExecutionState::Ready:
    expected = ExecutionState::Ready;
    if (state_.compare_exchange_strong(expected, ExecutionState::Running,
                                       std::memory_order_acq_rel)) {
      thread_id_ = std::this_thread::get_id();
      return true;
    }
    break;
  case ExecutionState::Running:
    expected = ExecutionState::Running;
    if (state_.compare_exchange_strong(expected, ExecutionState::Stopped,
                                       std::memory_order_acq_rel)) {
      thread_id_.reset();
      return true;
    }
    break;
  case ExecutionState::Stopped:
    break;
  }
  return false;
}

} // namespace sgrpc
