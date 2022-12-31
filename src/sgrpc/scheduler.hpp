
#pragma once

#include "sender.hpp"

#include <unifex/scheduler_concepts.hpp>

namespace sgrpc {

class ExecutionContext;
using ::unifex::get_scheduler;

class Scheduler {
public:
  constexpr explicit Scheduler(ExecutionContext& context) noexcept : context_(context) {}

  constexpr bool operator==(const Scheduler& other) const noexcept {
    return &context_ == &other.context_;
  }
  constexpr bool operator!=(const Scheduler& other) const noexcept { return !(*this == other); }

  constexpr SchedulerSender schedule() const noexcept { return {context_}; }

  constexpr ExecutionContext& context() const noexcept { return context_; }

  friend SchedulerSender tag_invoke(unifex::tag_t<unifex::schedule>,
                                    const Scheduler& self) noexcept {
    return self.schedule();
  }

private:
  ExecutionContext& context_; // not-owned
};

} // namespace sgrpc

// namespace unifex::_schedule {
// constexpr auto schedule(const sgrpc::Scheduler& self) noexcept { return self.schedule(); }

// } // namespace unifex::_schedule
