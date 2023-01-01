
#pragma once

#include "inflight_rpc.hpp"

namespace sgrpc {

/**
 * Grpc interface stubs have factory functions for generating new async rpc request.
 * In particular, they combine `client_context, request, completion_queue` to create
 * a new response_reader.
 *
 * Can be created as so:
 * ~~~~~~
 * ResponseReaderFactory factory{&Service::PrepareAsyncSayHello};
 * ~~~~~~
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

} // namespace sgrpc
