
#pragma once

#include "completion_queue_event.hpp"

#include <grpcpp/alarm.h>

namespace sgrpc::detail {

class Alarm final : public sgrpc::CompletionQueueEvent {
public:
  Alarm(grpc::CompletionQueue& cq, std::function<void(bool)> thunk, gpr_timespec deadline)
      : thunk_{std::move(thunk)} {
    alarm_.Set(&cq, deadline, this);
  }

  void complete(bool is_ok) override {
    if (thunk_) {
      thunk_(is_ok);
    }
  }

private:
  std::function<void(bool)> thunk_;
  grpc::Alarm alarm_;
};

} // namespace sgrpc::detail
