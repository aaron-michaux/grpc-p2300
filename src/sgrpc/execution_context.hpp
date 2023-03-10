
#pragma once

#include "detail/atomic_task_stealing_queue.hpp"
#include "detail/completion_queue_event.hpp"
#include "detail/server_interface.hpp"

#include <grpcpp/completion_queue.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

namespace sgrpc
{

using ThunkType          = std::function<void()>;
using DeadlinedThunkType = std::function<void(bool)>;
using RpcFactory = std::function<std::unique_ptr<CompletionQueueEvent>(grpc::CompletionQueue&)>;

enum class ExecutionState : int { Ready = 0, Running, ShuttingDown, Stopped };

class ExecutionContext final
{
 public:
   //@{ Construction/Destruction
   ExecutionContext(unsigned n_threads, std::vector<std::unique_ptr<grpc::CompletionQueue>>&& cqs);
   ExecutionContext(unsigned n_threads, unsigned number_cqs);
   ExecutionContext(const ExecutionContext&) = delete;
   ExecutionContext(ExecutionContext&&)      = delete;
   ~ExecutionContext();
   ExecutionContext& operator=(const ExecutionContext&) = delete;
   ExecutionContext& operator=(ExecutionContext&&)      = delete;
   //@}

   //@{ Getters
   ExecutionState get_state() const noexcept { return state_.load(std::memory_order_acquire); }
   bool is_stopped() const noexcept { return get_state() == ExecutionState::Stopped; }
   //@}

   //@{ Mutation
   /**
    * This method must be called before `run()`. The attached server's lifetime is
    * extended until after `stop()`.
    */
   void attach_server(std::shared_ptr<ServerContainerInterface> server);
   //@}

   //@{ Posting events
   bool post(ThunkType thunk);
   bool post(DeadlinedThunkType thunk, std::chrono::steady_clock::time_point deadline);
   bool post(DeadlinedThunkType thunk, std::chrono::nanoseconds delta);
   bool post(RpcFactory call_factory);
   //@}

   //@{ Action!
   void run();                                      //!< Returns immediately
   void run_while(std::function<bool()> predicate); //!< Returns immediately
   void stop();                                     //!< Blocking waits for orderly shutdown
   void add_notify_at_stopped(std::function<void()> thunk);
   //@}

 private:
   enum class CqExecutionResult { ExecutedNone, ExecutedOne, ShutdownRequested };

   bool set_state_(ExecutionState state); //!< True iff successful
   grpc::CompletionQueue& get_cq_(unsigned index) const noexcept;
   grpc::CompletionQueue& get_next_cq_() const noexcept;
   void run_one_thread_(unsigned thread_number, std::function<bool()> predicate);
   CqExecutionResult execute_cq_(grpc::CompletionQueue& cq);

   //@{ Members
   mutable std::mutex padlock_;
   std::vector<std::thread> threads_;
   AtomicTaskStealingQueue<ThunkType> task_queue_; //!< For things not pushed onto cqs_
   std::vector<std::unique_ptr<grpc::CompletionQueue>> cqs_;
   std::vector<std::shared_ptr<ServerContainerInterface>> servers_;

   std::vector<ThunkType> notifications_; //!< For when stopped and drained
   std::atomic<ExecutionState> state_{ExecutionState::Ready};
   mutable std::atomic<std::size_t> next_cq_write_index_{0};
   mutable std::atomic<std::size_t> within_cq_post_{0};
   unsigned n_threads_{0};
   //@}
};

} // namespace sgrpc
