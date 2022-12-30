
#pragma once

#include "execution_context.hpp"

namespace sgrpc {

class Scheduler {
public:
  constexpr explicit Scheduler(ExecutionContext& context) noexcept : context_(context) {}

  constexpr bool operator==(const Scheduler& other) const noexcept {
    return &context_ == &other.context_;
  }
  constexpr ExecutionContext& context() const noexcept { return context_; }

  //[[nodiscard]] detail::ScheduleSender schedule() const noexcept;

private:
  ExecutionContext& context_; // not-owned
};

} // namespace sgrpc
