
#pragma once

// template <typename Service, typename PrepareAsyncMemFun, typename RequestType,
//           typename ResponseType>
// class RpcCall {
// public:
//   void send_to_wire() {
//     response_reader_ = reader_factory_(service_, &client_context_, request_, &cq_);
//     response_reader_->StartCall();
//     response_reader_->Finish(&response_, &status, this); // scheduled
//   }

//   RequestType& request() noexcept { return request_; }
//   const RequestType& request() const noexcept { return request_; }

// private:
//   Service& service_;
//   PrepareAsyncMemFun reader_factory_;
//   grpc::ClientContext client_context_;
//   grpc::Status status_;
//   RequestType request_;
//   ResponseType response_;
//   std::unique_ptr<grpc::ClientAsyncResponseReader<ResponseType>> response_reader_;
// };
