
#include "stdinc.hpp"

#include "greeting-grpc/greeting-client.h"
#include "greeting-grpc/greeting-server.h"

#include "sgrpc/execution_context.hpp"
#include "sgrpc/scheduler.hpp"

#include <stdexec/execution.hpp>
#include <exec/static_thread_pool.hpp>

#include <thread>

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

class SomeClass {
public:
  void fn(int b) { fmt::print("b = {}\n", b); }
};

int compute(int x) { return x + 1; }

int main(int, char**) {

  {
    SomeClass object;
    void (SomeClass::*ptr)(int) = &SomeClass::fn;
    (object.*ptr)(10);
  }

  std::thread server_thread{GreetingServer::run_server};

  sgrpc::ExecutionContext ctx{2, 1};
  ctx.run();

  stdexec::scheduler auto sched = sgrpc::Scheduler{ctx};

  // Describe some work:
  auto fun = [](int i) { return compute(i); };
  auto work = stdexec::when_all(stdexec::on(sched, stdexec::just(0) | stdexec::then(fun)),
                                stdexec::on(sched, stdexec::just(1) | stdexec::then(fun)),
                                stdexec::on(sched, stdexec::just(2) | stdexec::then(fun)));

  // Launch the work and wait for the result:
  auto [i, j, k] = stdexec::sync_wait(std::move(work)).value();

  // Print the results:
  fmt::print("{}, {}, {}\n", i, j, k);

  auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());
  Greeting::Client client{ctx, channel};
  client.say_hello("Pius");
  auto result = client.sync_say_hello("Metellus");
  fmt::print("{}\n", result);

  server_thread.join();

  return EXIT_SUCCESS;
}
