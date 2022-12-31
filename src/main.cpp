
#include "stdinc.hpp"

#include "greeting-grpc/greeting-client.h"
#include "greeting-grpc/greeting-server.h"

#include "sgrpc/execution_context.hpp"
#include "sgrpc/scheduler.hpp"

#include <unifex/single_thread_context.hpp>
#include <unifex/execute.hpp>
#include <unifex/scheduler_concepts.hpp>

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

int main(int, char**) {

  // sgrpc::ExecutionContext ctx{2, 1};
  // unifex::execute(sgrpc::Scheduler{ctx}, []() { fmt::print("Hello World!\n"); });

  unifex::single_thread_context ctx;
  unifex::execute(ctx.get_scheduler(), []() { fmt::print("Hello World!\n"); });

  std::thread server_thread{GreetingServer::run_server};
  GreetingClient::run_client();
  server_thread.join();

  return EXIT_SUCCESS;
}
