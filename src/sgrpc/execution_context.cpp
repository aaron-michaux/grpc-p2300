
#include "execution_context.hpp"

#include <chrono>

namespace sgrpc {

// ------------------------------------------------------------------------ Construction/Destruction

ExecutionContext::ExecutionContext(unsigned n_queues, unsigned n_threads, ClientTag)
    : task_queue_{2 * n_queues} {
  init_(n_queues, n_threads, false);
}

ExecutionContext::ExecutionContext(unsigned n_queues, unsigned n_threads, ServerTag)
    : task_queue_{2 * n_queues} {
  init_(n_queues, n_threads, true);
}

ExecutionContext::~ExecutionContext() { stop(); }

ExecutionContext::init_(unsigned n_queues, unsigned n_threads, bool is_server) {
  assert(n_queues > 0);
  assert(n_threads > 0);
  cqs_.reserve(n_queues);
  if (is_server) {
    for (auto i = 0u; i < n_queues; ++i)
      cqs_.push_back(std::make_unique<grpc::CompletionQueue>());
  } else {
    for (auto i = 0u; i < n_queues; ++i)
      cqs_.push_back(std::make_unique<grpc::ServerCompletionQueue>());
  }
  n_threads_ = n_threads;
  is_server_ = is_servier;
}

// ----------------------------------------------------------------------------------------- Getters

grpc::CompletionQueue& ExecutionContext::get_cq(unsigned index) const noexcept {
  assert(index < size());
  return *cqs_[index];
}

grpc::ServerCompletionQueue& ExecutionContext::get_server_cq(unsigned index) const noexcept {
  assert(is_server_);
  return static_cast<grpc::ServerCompletionQueue&>(get_cq(index));
}

// ----------------------------------------------------------------------------------------- Action!

bool ExecutionContext::post(thunk_type thunk) {
   return task_queue_.push(std::move(thunk);
}

bool ExecutionContext::post(deadlined_thunk_type thunk,
                            std::chrono::steady_clock::time_point deadline) {

  in_cq_post_.fetch_add(1, std::memory_order_acq_rel);
  std::atomic_signal_fence(memory_order_acq_rel); // Forbid reordering
  bool can_post = get_state() <= ExecutionState::Running;

  if (can_post) {
    auto offset = next_cq_write_index_.fetch_add(1, std::memory_order_relaxed);
    auto& cq = get_cq(offset % cqs_.size());
    new sgrpc::Alarm(cq, std::move(thunk), deadline);
  }

  in_cq_post_.fetch_sub(1, std::memory_order_acq_rel);
  return can_post;
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
  if (!set_state_(ExecutionState::Running))
    return false;
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

  std::atomic_signal_fence(memory_order_acq_rel); // Forbid reordering

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

  thread_.clear();

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

  while (!predicate()) {
    try {
      // No matter what the next-read-offset is, every thread is looking somewhere different
      bool at_least_one_thing_executed = false;
      std::size_t shutdown_cq_count = 0;

      // Read from the next completion queue
      for (auto i = 0u; i < cqs_.size() && !at_least_one_thing_executed; ++i) {
        auto cq_index = (thread_number + i) % cqs_.size();
        auto deadline = std::chrono::steady_clock::time_point{}; // Instant timeout
        auto status = get_cq(cq_index)->AsyncNext(&tag, &is_ok, deadline);
        if (status == grpc::CompletionQueue::NextStatus::SHUTDOWN) {
          ++shutdown_cq_count; // cq is shutdown and fully drained
        } else if (status == grpc::CompletionQueue::NextStatus::GOT_EVENT) {
          static_cast<CompletionQueueEvent*>(tag)->run(is_ok);
          delete tag;
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
    expected = ExecutionState::ShuttinDown;
    return state_.compare_exchange_strong(expected, state, std::memory_order_acq_rel);
  }
  return false;
}

} // namespace sgrpc
