
#pragma once

#include <grpcpp/completion_queue.h>

#include <atomic>
#include <optional>
#include <memory>
#include <mutex>
#include <thread>

namespace sgrpc {

enum class ExecutionState : int { Ready, Running, Stopped };

class ExecutionContext {
public:
  // For tag dispatch in the constructor
  static struct ClientTag {
  } Client;
  static struct ServerTag {
  } Server;

  //@{ Construction/Destruction
  explicit ExecutionContext(ClientTag);
  explicit ExecutionContext(ServerTag);
  //@}

  //@{ Getters
  ExecutionState get_state() const noexcept { return state_.load(std::memory_order_acquire); }
  bool is_stopped() const noexcept { return get_state() == ExecutionState::Stopped; }
  bool is_server() const noexcept { return is_server_; }
  bool is_client() const noexcept { return !is_server_; }
  grpc::CompletionQueue& get_cq() const noexcept { return *cq_; }
  grpc::ServerCompletionQueue* get_server_cq() const noexcept;
  bool is_running_in_thread(std::thread::id id) const noexcept;
  //@}

  //@{ Action!
  bool run();
  bool run_while(std::function<bool()> predicate);
  void stop();
  //@}

private:
  bool set_state(ExecutionState state); //!< True iff successful

  std::unique_ptr<grpc::CompletionQueue> cq_;
  std::atomic<ExecutionState> state_{ExecutionState::Ready};
  std::optional<std::thread::id> thread_id_{};
  bool is_server_{false};
};

} // namespace sgrpc
