
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
std::tuple<int, int, int, std::string> sumit(int x, int y, int z, std::string s)
{
   return {x, x + y, x + y + z, s};
}

namespace js
{
template<class Tag, class... Ts>
using completion_signatures_ = stdexec::completion_signatures<Tag(Ts...)>;

template<class _ReceiverId, class Tag, class... Ts> struct Operation
{
   using _Receiver = stdexec::__t<_ReceiverId>;

   struct OpState
   {
      using __id = Operation;
      std::tuple<Ts...> values;
      _Receiver __rcvr_;

      friend void tag_invoke(stdexec::start_t, OpState& op_state) noexcept
      {
         std::apply(
             [&op_state](Ts&... ts) { Tag{}(std::move(op_state.__rcvr_), std::move(ts)...); },
             op_state.values);
      }
   };
};
} // namespace js

int main(int, char**)
{
   std::thread server_thread{Greeting::run_server};

   sgrpc::ExecutionContext ctx{2, 1};
   stdexec::scheduler auto sched = sgrpc::Scheduler{ctx};

   auto channel = grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials());

   Greeting::Client client{ctx, channel}; // TODO: create with a scheduler (?)
   ctx.run();

   stdexec::sender auto snd = client.say_hello("Tritarch");

   // Describe some work:
   auto fun  = [](int i) { return compute(i); };
   auto work = stdexec::when_all(stdexec::on(sched, stdexec::just(0) | stdexec::then(fun)),
                                 stdexec::on(sched, stdexec::just(1) | stdexec::then(fun)),
                                 stdexec::on(sched, stdexec::just(2) | stdexec::then(fun)),
                                 snd)
               | stdexec::then(sumit);

   // Launch the work and wait for the result:
   auto [i, j, k, s] = std::get<0>(stdexec::sync_wait(std::move(work)).value());

   auto r2 = stdexec::sync_wait(std::move(snd));

   std::string response = std::get<0>(r2.value());

   // Print the results:
   fmt::print("{}, {}, {}, {}\n", i, j, k, s);
   fmt::print("response: {}\n", response);

   server_thread.join();

   return EXIT_SUCCESS;
}
