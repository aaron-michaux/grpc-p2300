
#include "stdinc.hpp"

#include "greeting-grpc/greeting-client.h"
#include "greeting-grpc/greeting-server.h"

#include "sgrpc/execution_context.hpp"
#include "sgrpc/scheduler.hpp"

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>

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

class SomeClass
{
 public:
   void fn(int b) { fmt::print("b = {}\n", b); }
};

int compute(int x) { return x + 1; }

std::string sumit(int x, int y, int z, std::string s)
{
   return fmt::format("({}, {}, {}), {}\n", x, y, z, s);
}

int main(int, char**)
{
   sgrpc::ExecutionContext ctx{2, 1};
   stdexec::scheduler auto sched = sgrpc::Scheduler{ctx};
   auto server = Greeting::ServerContainer::build(ctx, std::make_shared<Greeting::Server>());
   ctx.run();
   fmt::print("server listening on port:{}\n", server.port());

   auto channel = grpc::CreateChannel(fmt::format("localhost:{}", server.port()),
                                      grpc::InsecureChannelCredentials());
   Greeting::Client client{ctx, channel}; // TODO: create with a scheduler (?)

   stdexec::sender auto snd = client.say_hello("Tritarch");

   // Describe some work:
   auto fun = [](int i) { return compute(i); };
   stdexec::sender auto work
       = stdexec::when_all(stdexec::on(sched, stdexec::just(0) | stdexec::then(fun)),
                           stdexec::on(sched, stdexec::just(1) | stdexec::then(fun)),
                           stdexec::on(sched, stdexec::just(2) | stdexec::then(fun)),
                           snd)
         | stdexec::then(sumit)
         | stdexec::let_value([&](std::string s) { return client.say_hello(s); })
         | stdexec::upon_error([](auto... arg) {
              // fmt::print("status {}, {}", status.error_message(), status.details());
              // fmt::print(args...);
              return std::string{"there was an error of some kind"};
           });

   // Launch the work and wait for the result:
   auto [result] = stdexec::sync_wait(std::move(work)).value();

   auto r2 = stdexec::sync_wait(std::move(snd));

   std::string response = std::get<0>(r2.value());

   // Print the results:
   fmt::print("result:   {}\n", result);
   fmt::print("response: {}\n", response);

   return EXIT_SUCCESS;
}
