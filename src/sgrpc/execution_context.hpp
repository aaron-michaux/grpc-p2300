
#pragma once

#include "detail/atomic-task-stealing-queue.hpp"
#include "completion-queue-event.hpp"

#include <grpcpp/completion_queue.h>

#include <atomic>
#include <optional>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace sgrpc {

using thunk_type = std::function<void()>;
using deadlined_thunk_type = std::function<void(bool)>;

enum class ExecutionState : int { Ready = 0, Running, ShuttingDown, Stopped };

class ExecutionContext final {
public:
  //@{ Construction/Destruction
  ExecutionContext(unsigned n_threads, std::vector<std::unique_ptr<grpc::CompletionQueue>>&& cqs);
  ExecutionContext(unsigned n_threads, unsigned number_cqs);
  ExecutionContext(const ExecutionContext&) = delete;
  ExecutionContext(ExecutionContext&&) = delete;
  ~ExecutionContext();
  ExecutionContext& operator=(const ExecutionContext&) = delete;
  ExecutionContext& operator=(ExecutionContext&&) = delete;
  //@}

  //@{ Getters
  ExecutionState get_state() const noexcept { return state_.load(std::memory_order_acquire); }
  bool is_stopped() const noexcept { return get_state() == ExecutionState::Stopped; }
  grpc::CompletionQueue& get_cq(unsigned index) const noexcept;
  std::size_t size() const noexcept { return cqs_.size(); }
  //@}

  //@{ Action!
  bool post(thunk_type thunk);
  bool post(deadlined_thunk_type thunk, std::chrono::steady_clock::time_point deadline);
  bool post(deadlined_thunk_type thunk, std::chrono::nanoseconds delta);
  void run();                                      //!< Returns immediately
  void run_while(std::function<bool()> predicate); //!< Returns immediately
  void stop();                                     //!< Blocking waits for orderly shutdown
  void add_notify_at_stopped(std::function<void()> thunk);
  //@}

private:
  void init_(unsigned n_queues, unsigned n_threads, bool is_server);
  bool set_state_(ExecutionState state); //!< True iff successful
  void run_one_thread_(unsigned thread_number, std::function<bool()> predicate);

  std::mutex padlock_;
  std::vector<std::thread> threads_;
  AtomicTaskStealingQueue<thunk_type> task_queue_; //!< For things not pushed onto cqs_
  std::vector<std::unique_ptr<grpc::CompletionQueue>> cqs_;
  std::vector<std::function<void()>> notifications_; //!< For when stopped and drained
  std::atomic<ExecutionState> state_{ExecutionState::Ready};
  std::atomic<std::size_t> next_cq_write_index_{0};
  std::atomic<std::size_t> in_cq_post_{0};
  unsigned n_threads_{0};
};

} // namespace sgrpc
