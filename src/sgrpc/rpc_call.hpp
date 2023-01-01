
#pragma once

#include "completion_queue_event.hpp"
#include "execution_context.hpp"

#include <grpcpp/grpcpp.h>

namespace sgrpc {

template <typename RequestType, typename ResponseType>
using AsyncReaderFactory =
    std::function<std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>(
        grpc::ClientContext& client_context_, RequestType request)>;

template <typename ResponseType>
using CompletionThunk =
    std::function<void(bool, const grpc::Status&, const ResponseType& response)>;

/**
 * An "in-flight" RPC call.
 */
template <typename RequestType, typename ResponseType> class RpcCall : public CompletionQueueEvent {
public:
  RpcCall(AsyncReaderFactory<RequestType, ResponseType> response_reader_factory,
          RequestType request, CompletionThunk<ResponseType> thunk)
      : completion_{std::move(thunk)} {
    response_reader_ = response_reader_factory(client_context_, std::move(request));
    response_reader_->StartCall();
    response_reader_->Finish(&response_, &status_, this); // scheduled
    // the underlying grpc machinery now owns the lifecycle of `this`
  }

  // The sgrpc::ExecutionContext calls this when the rpc call completes and also deletes call_frame
  void complete(bool is_ok) override {
    assert(completion_);
    completion_(is_ok, status_, response_);
  }

private:
  grpc::ClientContext client_context_;
  grpc::Status status_;
  ResponseType response_;
  std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> response_reader_;
  CompletionThunk<ResponseType> completion_;
};

/**
 * Grpc interface stubs have factory functions for generating new async rpc request.
 * In particular, they combine `client_context, request, completion_queue` to create
 * a new response_reader.
 *
 * Can be created as so:
 * ~~~
 * ResponseReaderFactory factory{&Service::PrepareAsyncSayHello};
 * ~~~
 */
template <typename Service, typename RequestType, typename ResponseType>
struct ResponseReaderFactory {
  using service_type = Service;
  using request_type = RequestType;
  using response_type = ResponseType;
  using response_reader_type = grpc::ClientAsyncResponseReader<ResponseType>;
  using mem_func_ptr_type = std::unique_ptr<response_reader_type> (service_type::*)(
      grpc::ClientContext*, const request_type&, grpc::CompletionQueue*);

  // Want to implicitly construct from a service stub member function
  ResponseReaderFactory(Service& service, mem_func_ptr_type factory_fn)
      : service_{service}, factory_fn_{factory_fn} {}

  std::unique_ptr<response_reader_type> operator()(grpc::ClientContext* client_context,
                                                   const request_type& request,
                                                   grpc::CompletionQueue* cq) {
    return (service_.*factory_fn_)(client_context, request, cq);
  }

private:
  Service& service_;
  mem_func_ptr_type factory_fn_;
};

/**
 * The "stub" for the client side of an RpcCall. The `call` method is used to
 * generate new "in-flight" RPC calls that execution on the passed context.
 *
 * Rationale:
 * + The context must own its `completion_queues`, so that it can process events,
 *   and manage memory.
 * + If the context is shutting down (or already shutdown) we cannot create a new
 *   rpc call. This would cause a memory leak.
 * + Thus the context (and only the context) must control if the RPC call is generated,
 *   and which `completion_queue` it should be attached to.
 * + The `RpcCallStub::call` method supplies the call factory to `context`, which
 *   creates the call if it is able to.
 * + The `factory` in `RpcCallStub::call` is responsible for creating the RPC's response
 *   writer, which is an internal part of the RPC call itself. The response write
 *   need to know the `completion_queue`, which is passed into the call-factory, from
 *   the `context`.
 */
template <typename Service, typename RequestType, typename ResponseType> class RpcCallStub {
public:
  template <typename MemberFunctionPointer>
  RpcCallStub(Service& service, MemberFunctionPointer mem_fn_ptr)
      : factory_fn_{service, mem_fn_ptr} {}

  // Should return a sender... hopefully does not need access to the GrpcContext here
  void call(sgrpc::ExecutionContext& context, RequestType request,
            CompletionThunk<ResponseType> thunk) {
    context.post([this, &context, request = std::move(request), thunk = std::move(thunk)](
                     grpc::CompletionQueue& cq) mutable -> std::unique_ptr<CompletionQueueEvent> {
      // Factory for creating the correct response writer type
      auto factory = [this, &cq](grpc::ClientContext& client_context, RequestType request) mutable {
        return factory_fn_(&client_context, std::move(request), &cq);
      };
      return std::make_unique<RpcCall<RequestType, ResponseType>>(
          std::move(factory), std::move(request), std::move(thunk));
    });
  }

private:
  ResponseReaderFactory<Service, RequestType, ResponseType> factory_fn_;
};

} // namespace sgrpc
