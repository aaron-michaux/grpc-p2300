
#pragma once

#include "stdinc.hpp"

#include "detail/base_inc.hpp"
#include "detail/rpc_sender_operation_states.hpp"

#include "execution_context.hpp"
#include "rpc_status.hpp"
#include "scheduler.hpp"

namespace sgrpc
{

/**
 * A Sender for grpc RPCs that uses the raw input/output protobuf types.
 */
template<typename Service, typename RequestType, typename ResponseType> class PureClientRpcSender
{
 private:
   /**
    * OperationState connect(RpcSender self, Receiver receiver)
    */
   template<class R>
   friend auto tag_invoke(stdexec::connect_t, PureClientRpcSender self, R&& receiver)
   {
      return detail::
          PureRpcSenderOpState<Service, RequestType, ResponseType, std::remove_cvref_t<R>>{
              self.context_,
              std::move(self.factory_fn_),
              std::move(self.request_),
              std::move(receiver)};
   }

   /**
    * Scheduler get_completion_scheduler(RpcSender self)
    */
   friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                               PureClientRpcSender self) noexcept
   {
      return Scheduler{self.context_};
   }

 public:
   using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(ResponseType),
                                                                stdexec::set_error_t(grpc::Status)>;

   PureClientRpcSender(ExecutionContext& context,
                       ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn,
                       RequestType request)
       : context_{context}
       , factory_fn_{std::move(factory_fn)}
       , request_{std::move(request)}
   {}

 private:
   ExecutionContext& context_;
   ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
   std::shared_ptr<RequestType> request_;
};

/**
 * A type-erased RpcSender: only knows about the (wrapped) ResultType; no Service/Protobuf
 */
template<typename ResultType> class ClientRpcSender
{
 private:
   /**
    * OperationState connect(RpcSender self, Receiver receiver)
    */
   template<class R> friend auto tag_invoke(stdexec::connect_t, ClientRpcSender self, R&& receiver)
   {
      return detail::RpcSenderOpState<std::remove_cvref_t<R>, ResultType>{
          self.context_, std::move(self.call_factory_), std::move(receiver)};
   }

   /**
    * Scheduler get_completion_scheduler(RpcSender self)
    */
   friend Scheduler tag_invoke(stdexec::get_completion_scheduler_t<stdexec::set_value_t>,
                               ClientRpcSender self) noexcept
   {
      return Scheduler{self.context_};
   }

 public:
   using completion_signatures = stdexec::completion_signatures<stdexec::set_value_t(ResultType),
                                                                stdexec::set_error_t(RpcStatus)>;

   ClientRpcSender(ExecutionContext& context, WrappedRpcFactory<ResultType> call_factory)
       : context_{context}
       , call_factory_{std::move(call_factory)}
   {}

 private:
   ExecutionContext& context_;
   WrappedRpcFactory<ResultType> call_factory_;
};

} // namespace sgrpc
