
#include "stdinc.hpp"

#include "greeting-grpc/greeting-client.h"
#include "greeting-grpc/greeting-server.h"

#include "sgrpc/execution_context.hpp"
#include "sgrpc/scheduler.hpp"

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

#include <thread>

// -------------------------------------------------------------------------------------------------
// // std::tag_invoke
// // std::tag_t
// // std::tag_invoke_result_t
// namespace mylib {
// // The CPO object
// inline constexpr struct foo_fn {
//   template <typename T>
//   // Trailing return type, because different CPOs can return different types. So
//   // this goes through automatic type deduction.
//   auto operator()(const T& x) const -> std::tag_invoke_result_t<foo_fn, const T&> {
//     return std::tag_invoke(*this, x);
//   }
// } foo{};

// // This is the CPO
// template <typename T>
// requires std::invocable<decltype(mylib::foo), const T&> bool print_foo(const T& value) {
//   std::cout << mylib::foo(value) << std::endl;
// }
// } // namespace mylib

// namespace otherlib {

// class other_type {
// private:
//   // Opt-in to CPO through ADL
//   friend int tag_invoke(std::tag_t<mylib::foo>, const other_type& x) { return x.value_; }
//   int value_{0};
// };
// } // namespace otherlib

// -------------------------------------------------------------------------------------------------
/**
 * concept scheduler:
 *    schedule(scheduler) -> sender
 *
 * concept sender:
 *    connect(sender, receiver) -> operation_state
 *
 * concept receiver:
 *    set_value(reciever, Values...) -> void
 *    set_value(receiver, Error...) -> void
 *    set_stopped(receiver) -> void
 *
 * concept operation_state:
 *    start(operation_state) -> void
 */

class inline_scheduler {
  using ExecutionContext = sgrpc::ExecutionContext;

  template <class R_> struct _op {
    using R = stdexec::__t<R_>;
    ExecutionContext& context_;
    [[no_unique_address]] R receiver_;

    void start() noexcept {
      try {
        stdexec::set_value(std::move(receiver_));
      } catch (...) {
        stdexec::set_error(std::move(receiver_), std::current_exception());
      }
    }

    friend void tag_invoke(stdexec::start_t, _op& self) noexcept {
      auto thunk = [self]() mutable { self.start(); };
      self.context_.post([thunk]() mutable { thunk(); });
    }
  };

  struct _sender {
    using completion_signatures =
        stdexec::completion_signatures<stdexec::set_value_t(),
                                       stdexec::set_error_t(std::exception_ptr)>;

    explicit _sender(ExecutionContext& context) : context_{context} {}

    template <class R>
    friend auto tag_invoke(stdexec::connect_t, _sender self, R&& rec)
        -> _op<std::remove_cvref_t<R>> {
      return {self.context_, std::move(rec)};
    }

    friend inline_scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                                       _sender self) noexcept {
      return inline_scheduler{self.context_};
    }

    ExecutionContext& context_;
  };

public:
  explicit inline_scheduler(ExecutionContext& context) : context_{context} {}
  friend _sender tag_invoke(stdexec::schedule_t, inline_scheduler self) noexcept {
    return _sender{self.context_};
  }
  bool operator==(const inline_scheduler& o) const noexcept { return &context_ == &o.context_; }

  ExecutionContext& context_;
};

int compute(int x) { return x + 1; }

int main(int, char**) {

  // sgrpc::ExecutionContext ctx{2, 1};
  // unifex::execute(sgrpc::Scheduler{ctx}, []() { fmt::print("Hello World!\n"); });

  // Get a handle to the thread pool:
  // exec::static_thread_pool pool(8);
  // stdexec::scheduler auto sched = pool.get_scheduler();
  sgrpc::ExecutionContext ctx{2, 1};
  ctx.run();
  stdexec::scheduler auto schedz = inline_scheduler{ctx};

  stdexec::scheduler auto sched = sgrpc::Scheduler{ctx};

  // Describe some work:
  auto fun = [](int i) { return compute(i); };
  auto work = stdexec::when_all(stdexec::on(sched, stdexec::just(0) | stdexec::then(fun)),
                                stdexec::on(sched, stdexec::just(1) | stdexec::then(fun)),
                                stdexec::on(sched, stdexec::just(2) | stdexec::then(fun)));

  // Launch the work and wait for the result:
  auto [i, j, k] = stdexec::sync_wait(std::move(work)).value();

  // Print the results:
  fmt::print("{}, {}, {}", i, j, k);

  std::thread server_thread{GreetingServer::run_server};
  GreetingClient::run_client();
  server_thread.join();

  return EXIT_SUCCESS;
}
