
#pragma once

#include "detail/base_inc.hpp"
#include "scheduler.hpp"

namespace sgrpc
{

namespace detail
{
   template<typename ReceiverType, typename ResultType> class GenderOperationState
   {
      [[no_unique_address]] ReceiverType receiver_;

      GenderOperationState(GenderOperationState&&)            = delete;
      GenderOperationState& operator=(GenderOperationState&&) = delete;

      friend void tag_invoke(stdexec::start_t, GenderOperationState& self) noexcept
      {
         self.invoke();
      }
   };
} // namespace detail

template<typename ResultType> class GenericSender
{
 private:
   /**
    * OperationState connect(RpcSender self, Receiver receiver)
    */
   template<class R> friend auto tag_invoke(stdexec::connect_t, GenericSender self, R&& receiver)
   {
      return detail::GenderOperationState<std::remove_cvref_t<R>, ResultType>{
          self.context_, std::move(self.call_factory_), std::move(receiver)};
   }

   /**
    * Scheduler get_completion_scheduler(RpcSender self)
    */
   friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                               GenericSender self) noexcept
   {
      return self.scheduler_;
   }

 public:
   using completion_signatures
       = stdexec::completion_signatures<stdexec::set_value_t(ResultType),
                                        stdexec::set_error_t(std::exception_ptr)>;

   GenericSender(Scheduler scheduler)
       : scheduler_{scheduler}
   {}

 private:
   Scheduler scheduler_;
};

} // namespace sgrpc
