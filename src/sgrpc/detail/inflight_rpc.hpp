
#pragma once

#include "sgrpc/execution_context.hpp"

#include "completion_queue_event.hpp"

#include <grpcpp/grpcpp.h>

namespace sgrpc
{

/**
 * Factory for creating the correct type of grpc response reader for a given
 * grpc::ClientContext. The factory method should internalize the request arguments.
 */
template<typename ResponseType>
using AsyncReaderFactory
    = std::function<std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>>(
        grpc::ClientContext& client_context)>;

template<typename ResponseType>
using CompletionThunk
    = std::function<void(bool, const grpc::Status&, const ResponseType& response)>;

/**
 * An "in-flight" RPC call; lives on the heap; lifecycle managed externally
 */
template<typename ResponseType> class InflightRpc : public CompletionQueueEvent
{
 public:
   InflightRpc(AsyncReaderFactory<ResponseType> response_reader_factory,
               CompletionThunk<ResponseType> thunk)
       : completion_{std::move(thunk)}
   {
      response_reader_ = response_reader_factory(client_context_);
      response_reader_->StartCall();
      response_reader_->Finish(&response_, &status_, this); // scheduled
      // To end the lifecycle of this object, the underlying grpc machinery now must
      // call `complete(...)`; this is grpc's memory management idiom.
   }

   // The sgrpc::ExecutionContext calls this when the rpc call completes and also deletes call_frame
   void complete(bool is_ok) noexcept override
   {
      assert(completion_);
      try {
         completion_(is_ok, status_, response_);
      } catch(...) {
         // TODO: log something here
      }
      delete this;
   }

 private:
   grpc::ClientContext client_context_;
   grpc::Status status_;
   ResponseType response_;
   std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> response_reader_;
   CompletionThunk<ResponseType> completion_;
};

} // namespace sgrpc
