
#pragma once

#include "sender.hpp"

#include "detail/base_inc.hpp"

namespace sgrpc {

class ExecutionContext;

class Scheduler {
  // OperationState: start()
  template <typename R> struct Op_ {
    ExecutionContext& context_;
    [[no_unique_address]] R receiver_;

    void start() noexcept {
      try {
        stdexec::set_value(std::move(receiver_));
      } catch (...) {
        stdexec::set_error(std::move(receiver_), std::current_exception());
      }
    }

    friend void tag_invoke(stdexec::start_t, Op_& self) noexcept {
      // The start of a computation chain on `context_`
      self.context_.post([self]() mutable { self.start(); });
    }
  };

  // Sender: connect(...), get_completion_scheduler(...)
  struct Sender_ {
    ExecutionContext& context_; // not-owned

    using completion_signatures =
        stdexec::completion_signatures<stdexec::set_value_t(),
                                       stdexec::set_error_t(std::exception_ptr)>;

    template <class R>
    friend auto tag_invoke(stdexec::connect_t, Sender_ self, R&& rec)
        -> Op_<std::remove_cvref_t<R>> {
      return {self.context_, std::move(rec)};
    }

    friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                                Sender_ self) noexcept {
      return Scheduler{self.context_};
    }
  };

  // Scheduler: schedule()
  friend Sender_ tag_invoke(stdexec::schedule_t, Scheduler self) noexcept {
    return self.schedule();
  }

public:
  constexpr explicit Scheduler(ExecutionContext& context) noexcept : context_(context) {}
  constexpr bool operator==(const Scheduler& o) const noexcept { return &context_ == &o.context_; };
  constexpr Sender_ schedule() const noexcept { return {context_}; }
  constexpr ExecutionContext& context() const noexcept { return context_; }

private:
  ExecutionContext& context_;
};

} // namespace sgrpc

// } // namespace unifex::_schedule
