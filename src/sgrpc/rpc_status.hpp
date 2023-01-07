
#pragma once

#include "detail/rpc_status_code_category.hpp"
#include "rpc_status_code.hpp"

#include <string>
#include <string_view>
#include <system_error>

/**
 * A wrapper for grpc::Status
 *
 * Rationale:
 * + The rpc_stub can produce "wrapped" RPC Senders that know nothing of the underlying
 *   grpc service, or protobuf types. To completely complete grpc+protobuf from client,
 *   we also must create a class to handle errors: i.e., we must wrap grpc::Status.
 * + Uses `expected` to take advantage of error-code features.
 */
namespace sgrpc
{

class RpcStatus
{
 public:
   explicit RpcStatus(RpcStatusCode error_code = RpcStatusCode::Ok)
       : error_code_{to_error_code(error_code)}
   {}
   RpcStatus(RpcStatusCode error_code, std::string message)
       : details_{std::move(message)}
       , error_code_{to_error_code(error_code)}
   {}
   RpcStatus(const RpcStatus& o) noexcept            = default;
   RpcStatus(RpcStatus&& o) noexcept                 = default;
   ~RpcStatus()                                      = default;
   RpcStatus& operator=(const RpcStatus& o) noexcept = default;
   RpcStatus& operator=(RpcStatus&& o) noexcept      = default;

   RpcStatusCode error_code() const { return static_cast<RpcStatusCode>(error_code_.value()); }
   std::string error_message() const { return error_code_.message(); }

   bool ok() const { return error_code() == RpcStatusCode::Ok; }

   std::string_view details() const { return details_; }

 private:
   std::error_code to_error_code(RpcStatusCode code)
   {
      return std::error_code{static_cast<int>(code), RpcStatusCodeCategory{}};
   }

   std::string details_{};
   std::error_code error_code_;
};

} // namespace sgrpc
