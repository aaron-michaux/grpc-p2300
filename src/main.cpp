
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

  std::thread server_thread{GreetingServer::run_server};

  sgrpc::ExecutionContext ctx{2, 1};
  stdexec::scheduler auto sched = sgrpc::Scheduler{ctx};

  auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

  Greeting::Client client{ctx, channel}; // TODO: create with a scheduler (?)
  ctx.run();

  helloworld::HelloRequest request;
  request.set_name("Pius!");

  // Describe some work:
  auto fun = [](int i) { return compute(i); };
  auto work = stdexec::when_all(stdexec::on(sched, stdexec::just(0) | stdexec::then(fun)),
                                stdexec::on(sched, stdexec::just(1) | stdexec::then(fun)),
                                stdexec::on(sched, stdexec::just(2) | stdexec::then(fun)));

  auto snd = client.say_hello(std::move(request));

  auto result = client.sync_say_hello("Metellus");
  fmt::print("{}\n", result);

  // Launch the work and wait for the result:
  auto [i, j, k] = stdexec::sync_wait(std::move(work)).value();

  stdexec::sync_wait(std::move(snd));

  // Print the results:
  fmt::print("{}, {}, {}\n", i, j, k);

  server_thread.join();

  return EXIT_SUCCESS;
}
