
#pragma once

#include <sgrpc/completion-queue-event.hpp>

#include <grpcpp/alarm.h>

namespace sgprc::detail {

class Alarm final : public CompletionQueueEvent {
public:
  Alarm(gprc::CompletionQueue& cq, std::function<void(bool)> thunk,
        std::chrono::steady_clock::time_point deadline)
      : thunk_{std::move(thunk)} {
    alarm_.Set(&cq, deadline, this);
  }

  void run(bool is_ok) override {
    if (thunk_) {
      thunk(is_ok);
    }
  }

private:
  std::function<void(bool)> thunk_;
  grpc::Alarm alarm_;
}

} // namespace sgprc::detail
