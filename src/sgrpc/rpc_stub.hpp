
#pragma once

#include "rpc_sender.hpp"
#include "inflight_rpc.hpp"
#include "response_reader_factory.hpp"

namespace sgrpc {

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
template <typename Service, typename RequestType, typename ResponseType> class RpcStub {
public:
  template <typename MemberFunctionPointer>
  RpcStub(Service& service, MemberFunctionPointer mem_fn_ptr) : factory_fn_{service, mem_fn_ptr} {}

  // The sender here is like a future.
  PureRpcSender<Service, RequestType, ResponseType> call(sgrpc::ExecutionContext& context,
                                                         RequestType request) {
    return {context, factory_fn_, std::move(request)};
  }

private:
  ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
};

} // namespace sgrpc
