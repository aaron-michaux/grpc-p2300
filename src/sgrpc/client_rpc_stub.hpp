
#pragma once

#include "rpc_sender.hpp"

namespace sgrpc
{

/**
 * The "stub" for the client side of an InflightRpc. The `call` method is used to
 * generate new "in-flight" RPC calls that execution on the passed context.
 *
 * Rationale:
 * + The context must own its `completion_queues`, so that it can process events,
 *   and manage memory.
 * + If the context is shutting down (or already shutdown) we cannot create a new
 *   rpc call. This would cause a memory leak.
 * + Thus the context (and only the context) must control if the RPC call is generated,
 *   and which `completion_queue` it should be attached to.
 * + The `RpcStub::call` method supplies the call factory to `context`, which
 *   creates the call if it is able to.
 * + The `factory` in `RpcStub::call` is responsible for creating the RPC's response
 *   writer, which is an internal part of the RPC call itself. The response write
 *   need to know the `completion_queue`, which is passed into the call-factory, from
 *   the `context`.
 */
template<typename Service, typename RequestType, typename ResponseType> class ClientRpcStub
{
 public:
   template<typename MemberFunctionPointer>
   ClientRpcStub(Service& service, MemberFunctionPointer mem_fn_ptr)
       : factory_fn_{service, mem_fn_ptr}
   {}

   /**
    * The sender here is like a future; The call sends/receives Protobuf envelopes
    */
   PureClientRpcSender<Service, RequestType, ResponseType> call(sgrpc::ExecutionContext& context,
                                                                RequestType request)
   {
      return {context, factory_fn_, std::move(request)};
   }

   /**
    * Wrap the RPC to type-erase the entire grpc service from the user of the RpcSender
    */
   template<typename ResultType,         // The unwrapped result type
            typename ConversionFunction> // Functor to convert from ResponseType => ResultType
   ClientRpcSender<ResultType> call(sgrpc::ExecutionContext& context, RequestType request)
   {
      using CallData
          = detail::CallData<Service, RequestType, ResponseType, ResultType, ConversionFunction>;

      WrappedRpcFactory<ResultType> factory
          = [data = std::make_shared<CallData>(context, factory_fn_, std::move(request))](
                WrappedCompletionHandler<ResultType> completion) mutable -> RpcFactory {
         data->completion = std::move(completion);
         return [data = std::move(data)](grpc::CompletionQueue& cq) mutable { return (*data)(cq); };
      };

      return {context, std::move(factory)};
   }

 private:
   ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
};

} // namespace sgrpc
