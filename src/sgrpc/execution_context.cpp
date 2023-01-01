
#include "execution_context.hpp"

#include "detail/alarm.hpp"
#include "detail/utils.hpp"

#include <chrono>

namespace sgrpc {

// ------------------------------------------------------------------------ Construction/Destruction

ExecutionContext::ExecutionContext(unsigned n_threads,
                                   std::vector<std::unique_ptr<grpc::CompletionQueue>>&& cqs)
    : task_queue_{n_threads + 1}, cqs_{std::move(cqs)}, n_threads_{n_threads} {
  assert(n_threads > 0);
  assert(cqs_.size() > 0);
}

ExecutionContext::ExecutionContext(unsigned n_threads, unsigned number_cqs)
    : task_queue_{n_threads + 1}, n_threads_{n_threads} {
  assert(n_threads > 0);
  assert(number_cqs > 0);
  cqs_.reserve(number_cqs);
  for (auto i = 0u; i < number_cqs; ++i)
    cqs_.push_back(std::make_unique<grpc::CompletionQueue>());
}

ExecutionContext::~ExecutionContext() { stop(); }

// ----------------------------------------------------------------------------------------- Getters

grpc::CompletionQueue& ExecutionContext::get_cq(unsigned index) const noexcept {
  assert(index < size());
  return *cqs_[index];
}

grpc::CompletionQueue& ExecutionContext::get_next_cq() const noexcept {
  auto offset = next_cq_write_index_.fetch_add(1, std::memory_order_relaxed);
  return get_cq(offset % cqs_.size());
}

// -------------------------------------------------------------------------------------------- Post

bool ExecutionContext::post(thunk_type thunk) { return task_queue_.push(std::move(thunk)); }

bool ExecutionContext::post(deadlined_thunk_type thunk,
                            std::chrono::steady_clock::time_point deadline) {

  const auto now = std::chrono::steady_clock::now();
  if (now <= deadline) {
    return post([thunk = std::move(thunk)]() { thunk(false); });
  }
  return post(std::move(thunk), deadline - now);
}

bool ExecutionContext::post(deadlined_thunk_type thunk, std::chrono::nanoseconds delta) {
  in_cq_post_.fetch_add(1, std::memory_order_acq_rel);
  std::atomic_signal_fence(std::memory_order_acq_rel); // Forbid reordering
  bool can_post = get_state() <= ExecutionState::Running;

  if (can_post) {
    new detail::Alarm{get_next_cq(), std::move(thunk), detail::duration_to_grp_timespec(delta)};
  }

  in_cq_post_.fetch_sub(1, std::memory_order_acq_rel);
  return can_post;
}

bool ExecutionContext::post(rpc_call_factory call_factory) {
  in_cq_post_.fetch_add(1, std::memory_order_acq_rel);
  std::atomic_signal_fence(std::memory_order_acq_rel); // Forbid reordering
  bool can_post = get_state() <= ExecutionState::Running;

  if (can_post) {
    call_factory(get_next_cq()).release();
  }

  in_cq_post_.fetch_sub(1, std::memory_order_acq_rel);
  return can_post;
}

// ----------------------------------------------------------------------------------------- Action!

/**
 * Runs until stopped
 */
void ExecutionContext::run() {
  run_while([]() { return false; });
}

/**
 * Runs until stopped or predicate returns `true`
 */
void ExecutionContext::run_while(std::function<bool()> predicate) {
  if (!set_state_(ExecutionState::Running))
    return;
  threads_.reserve(n_threads_);
  for (auto i = 0u; i < n_threads_; ++i)
    threads_.push_back(std::thread([this, i, predicate]() { run_one_thread_(i, predicate); }));
}

/**
 * Stops execution
 */
void ExecutionContext::stop() {
  bool shutdown_set = set_state_(ExecutionState::ShuttingDown); // X:  Write to M
  if (!shutdown_set)
    return;

  std::atomic_signal_fence(std::memory_order_acq_rel); // Forbid reordering

  // Busy wait until all post jobs are done
  while (in_cq_post_.load(std::memory_order_acquire) > 0) {
    std::this_thread::yield();
  }

  // Halt queues
  for (auto& cq : cqs_)
    cq->Shutdown();

  // Join threads
  for (auto& thread : threads_)
    thread.join();

  threads_.clear();

  // -- Now the notifications
  std::vector<std::function<void()>> notifications;
  {
    std::lock_guard lock{padlock_};
    set_state_(ExecutionState::Stopped);
    std::swap(notifications_, notifications);
  }
  for (const auto& thunk : notifications)
    thunk();
}

void ExecutionContext::run_one_thread_(unsigned thread_number, std::function<bool()> predicate) {
  void* tag = nullptr;
  bool is_ok = false;
  thunk_type thunk;

  while (true) {
    if (predicate()) { // Predicate causes 'stop()' to happen
      stop();          // Which causes all thunks to drain
    }
    try {
      // No matter what the next-read-offset is, every thread is looking somewhere different
      bool at_least_one_thing_executed = false;
      std::size_t shutdown_cq_count = 0;

      // Read from the next completion queue
      for (auto i = 0u; i < cqs_.size() && !at_least_one_thing_executed; ++i) {
        auto cq_index = (thread_number + i) % cqs_.size();
        auto deadline = std::chrono::system_clock::time_point{}; // Instant timeout
        auto status = get_cq(cq_index).AsyncNext(&tag, &is_ok, deadline);
        if (status == grpc::CompletionQueue::NextStatus::SHUTDOWN) {
          ++shutdown_cq_count; // cq is shutdown and fully drained
        } else if (status == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
          auto event = static_cast<CompletionQueueEvent*>(tag);
          event->complete(is_ok);
          delete event;
          at_least_one_thing_executed = true;
        }
      }

      if (shutdown_cq_count == cqs_.size()) {
        break; // switch to full shutdown mode
      }

      { // Read from the work queue
        if (task_queue_.try_pop(thunk)) {
          thunk();
          at_least_one_thing_executed = true;
        }
      }

      if (!at_least_one_thing_executed) { // Sleep if we've failed to execute anything
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
    } catch (...) {
      // FATAL: exception escaped thunk
      std::terminate();
    }
  }

  // We're in shutdown mode... draing everything from `task_queue_`
  auto thunks = task_queue_.stop_and_eject();
  for (auto& thunk : thunks) {
    thunk(); // execute these tasks "in-thread"
  }

  // And we're done
}

void ExecutionContext::add_notify_at_stopped(std::function<void()> thunk) {
  std::lock_guard lock{padlock_};
  if (get_state() < ExecutionState::Stopped) {
    notifications_.push_back(std::move(thunk));
  }
}

bool ExecutionContext::set_state_(ExecutionState state) {
  ExecutionState expected;
  switch (state) {
  case ExecutionState::Ready:
    return false;

  case ExecutionState::Running:
    expected = ExecutionState::Ready;
    return state_.compare_exchange_strong(expected, state, std::memory_order_acq_rel);

  case ExecutionState::ShuttingDown:
    expected = ExecutionState::Running;
    return state_.compare_exchange_strong(expected, state, std::memory_order_acq_rel);

  case ExecutionState::Stopped:
    expected = ExecutionState::ShuttingDown;
    return state_.compare_exchange_strong(expected, state, std::memory_order_acq_rel);
  }
  return false;
}

} // namespace sgrpc
