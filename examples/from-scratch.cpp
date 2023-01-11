
#include "stdinc.hpp"

#include <utility>

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

// -------------------------------------------------------------------------------------------------

struct immovable
{
   immovable()            = default;
   immovable(immovable&&) = delete;
};

// The result type of calling "connect(sender, receiver)"
template<typename Sender, typename Receiver>
using connect_result_t = decltype(connect(std::declval<Sender>(), std::declval<Receiver>()));

template<typename Sender> using sender_result_t = typename Sender::result_t;

// -------------------------------------------------------------------------------------------- just

/**
 * Takes an ordinary values, and turns it into a `sender`
 */
template<typename Receiver, typename ValueType> struct just_operation : immovable
{
   Receiver receiver;
   ValueType value;
   friend void start(just_operation& self) { set_value(self.receiver, self.value); }
};

template<typename ValueType> struct just_sender
{
   using result_t = ValueType;
   ValueType value_;
   template<typename Receiver>
   friend just_operation<Receiver, ValueType> connect(just_sender self, Receiver receiver)
   {
      return {{}, receiver, self.value};
   }
};

template<typename ValueType> just_sender<ValueType> just(ValueType t) { return {t}; }

// -------------------------------------------------------------------------------------------- then

template<typename Receiver, typename Function> struct then_receiver
{
   Receiver receiver;
   Function function;

   friend void set_value(then_receiver self, auto val)
   {
      set_value(self.receiver, self.function(val)); // set-value on outer-receiver
   }
   friend void set_error(then_receiver self, std::exception_ptr err)
   {
      set_error(self.receiver, err);
   }
   friend void set_stopped(then_receiver self) { set_stopped(self.receiver); }
};

template<typename Sender, typename Receiver, typename Function> struct then_operation : immovable
{
   connect_result_t<Sender, then_receiver<Receiver, Function>> op;
   friend void start(then_operation& self) { start(self.op); }
};

template<typename Sender, typename Function> struct then_sender
{
   using result_t = std::invoke_result_t<Function, sender_result_t<Sender>>;
   Sender sender;
   Function function;
   template<typename Receiver>
   friend then_operation<Sender, Receiver, Function> connect(then_sender self, Receiver receiver)
   {
      return {{}, connect(self.sender, then_receiver<Receiver, Function>(receiver, self.function))};
   }
};

template<typename Sender, typename Function>
then_sender<Sender, Function> then(Sender s, Function f)
{
   return {s, f};
}

// --------------------------------------------------------------------------------------- sync_wait

struct sync_wait_control_block
{
   std::mutex padlock;
   std::condition_variable cv;
   std::exception_ptr ex;
   bool is_done = false;
};

template<typename ValueType> struct sync_wait_receiver
{
   sync_wait_control_block& control;
   std::optional<ValueType>& value;

   friend void set_value(sync_wait_receiver self, auto val)
   {
      std::unique_lock lock{self.control.padlock};
      self.value.emplace(val);
      self.control.is_done = true;
      self.control.cv.notify_one();
   }

   friend void set_error(sync_wait_receiver self, std::exception_ptr ex)
   {
      std::unique_lock lock{self.control.padlock};
      self.control.ex      = ex;
      self.control.is_done = true;
      self.control.cv.notify_one();
   }

   friend void set_stopped(sync_wait_receiver self)
   {
      std::unique_lock lock{self.control.padlock};
      self.control.is_done = true;
      self.control.cv.notify_one();
   }
};

template<typename Sender> std::optional<sender_result_t<Sender>> sync_wait(Sender sender)
{
   using result_type = sender_result_t<Sender>;
   sync_wait_control_block control;
   std::optional<result_type> value;

   auto op = connect(sender, sync_wait_receiver<result_type>{control, value});
   start(op);
   std::unique_lock lock{control.padlock};
   control.cv.wait(lock, [&control]() { return control.is_done; });

   if(control.ex) std::rethrow_exception(control.ex);

   return value;
}

// ---------------------------------------------------------------------- run_loop execution context

struct run_loop : immovable
{
   struct none
   {}; // Not-a-value

   struct task : immovable
   {
      task* next = this;
      virtual void execute() {}
   };

   template<typename Receiver> struct operation : task
   {
      Receiver receiver;
      run_loop& loop;

      operation(Receiver receiver, run_loop& loop)
          : receiver{receiver}
          , loop{loop}
      {}

      void execute() override final { set_value(receiver, none{}); }

      friend void start(operation& self)
      {
         self.loop.push_back(&self); // insert operation into the queue
      }
   };

   task head;
   task* tail = &head;
   std::mutex padlock;
   std::condition_variable cv;
   bool finishing = false;

   void push_back(task* op)
   {
      std::unique_lock lock{padlock};
      op->next = &head; // circular linked list!
      tail = tail->next = op;
      cv.notify_one();
   }

   task* pop_front()
   {
      std::unique_lock lock{padlock};
      cv.wait(lock, [this]() { return head.next != &head || finishing; });
      if(head.next == &head) return nullptr;
      return std::exchange(head.next, head.next->next);
   }

   struct sender
   {
      using result_t = none;
      run_loop* loop;

      template<typename Receiver> friend operation<Receiver> connect(sender self, Receiver receiver)
      {
         return {receiver, *self.loop};
      }
   };

   struct scheduler
   {
      run_loop* loop;
      friend sender schedule(scheduler self) { return {self.loop}; }
   };

   scheduler get_scheduler() { return {this}; }

   void run()
   {
      while(auto* op = pop_front()) op->execute();
   }

   void finish()
   {
      std::unique_lock lock{padlock};
      finishing = true;
      cv.notify_all();
   }
};

// -------------------------------------------------------------------------------------------------

class thread_context : run_loop
{
   std::thread th{[this] { run(); }};

 public:
   using run_loop::finish;
   using run_loop::get_scheduler;
   void join() { th.join(); }
};

// ------------------------------------------------------------------------------- printing_receiver

struct printing_receiver
{
   friend void set_value(printing_receiver self, auto val) { std::print("Result: {}\n", val); }
   friend void set_error(printing_receiver self, std::exception_ptr err) { std::terminate(); }
   friend void set_stopped(printing_receiver self) { std::terminate(); }
};

// -------------------------------------------------------------------------------------------------

void example_1_inline()
{
   std::print("Example 1: senders and receives inline\n");

   auto s1  = just(42);
   auto op1 = connect(s1, printing_receiver{});
   start(op1);

   auto s2  = then(s1, [](int i) { return i + 1; });
   auto op2 = connect(s2, printing_receiver{});
   start(op2);

   auto s3  = then(s1, [](int i) { return i + 1; });
   int val3 = sync_wait(s3).value();
   std::print("Result3: {}\n", val3);
}

void example_2_scheduler()
{
   std::print("Example 2: a simple run loop scheduler\n");

   run_loop loop;
   auto sched4 = loop.get_scheduler();
   auto s4     = then(schedule(sched4), [](auto) { return 42; });
   auto op4    = connect(s4, printing_receiver{});
   start(op4);
   auto s5  = then(schedule(sched4), [](auto) { return 43; });
   auto op5 = connect(s5, printing_receiver{});
   start(op5);

   loop.finish();
   loop.run();
}

void example_3_threaded_execution()
{
   std::print("Example 3: run with a thread context\n");

   thread_context th;
   auto sched6 = th.get_scheduler();
   auto sx     = schedule(sched6);
   auto s6     = then(sx, [](auto) { return 42; });
   auto s7     = then(s6, [](int i) { return i + 1; });
   auto val7   = sync_wait(s7).value();

   // op-state7 contains op-state6, contains op-state-x
   // When op-state-x set_value(none) => op-state-6.set_value(42) => op-state-7.set_value(43)

   th.finish();
   th.join();

   std::print("Val7 = {}\n", val7);
}
