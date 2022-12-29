
#pragma once

#include "execution_context.hpp"

namespace sgrpc {

class Scheduler {
public:
  constexpr explicit Scheduler(ExecutionContext& context) noexcept : context_(context) {}

  [[nodiscard]] constexpr bool operator==(const Scheduler& other) const noexcept;
  [[nodiscard]] bool running_in_this_thread() const noexcept;
  [[nodiscard]] constexpr agrpc::GrpcExecutionContext& context() const noexcept { return context_; }

  //[[nodiscard]] detail::ScheduleSender schedule() const noexcept;

private:
  ExecutionContext& context_; // not-owned
};

} // namespace sgrpc
