
#pragma once

#include "sender.hpp"

namespace sgrpc {

class ExecutionContext;

class Scheduler {
public:
  constexpr explicit Scheduler(ExecutionContext& context) noexcept : context_(context) {}

  constexpr bool operator==(const Scheduler& other) const noexcept {
    return &context_ == &other.context_;
  }
  constexpr ExecutionContext& context() const noexcept { return context_; }

  constexpr SchedulerSender schedule() const noexcept { return {context_}; }

private:
  ExecutionContext& context_; // not-owned
};

} // namespace sgrpc
