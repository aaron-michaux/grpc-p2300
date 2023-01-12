
#pragma once

#include "sgrpc/rpc_status_code.hpp"

#include <system_error>

namespace sgrpc
{
struct RpcStatusCodeCategory : std::error_category
{
   const char* name() const noexcept override { return "rpc-status-code"; }
   std::string message(int e) const override
   {
      switch(static_cast<RpcStatusCode>(e)) {
      case RpcStatusCode::Ok: return "Success";
      case RpcStatusCode::Cancelled: return "The operation was cancelled, typically by the caller";
      case RpcStatusCode::Unknown: return "Unknown error";
      case RpcStatusCode::InvalidArgument: return "The client specified an invalid argument";
      case RpcStatusCode::DeadlineExceeded:
         return "The deadline expired before the operation could complete";
      case RpcStatusCode::NotFound:
         return "Some requested entity (e.g., file or directory) was not found";
      case RpcStatusCode::AlreadyExists:
         return "The entity that a client attempted to create (e.g., file or directory) already "
                "exists";
      case RpcStatusCode::PermissionDenied: return "Permission denied";
      case RpcStatusCode::ResourceExhausted:
         return "Some resource has been exhausted, perhaps a per-user quota, or perhaps the entire "
                "file system is out of space";
      case RpcStatusCode::FailedPrecondition:
         return "The operation was rejected because the system is not in a state required for the "
                "operation's execution";
      case RpcStatusCode::Aborted: return "The operation was aborted";
      case RpcStatusCode::OutOfRange:
         return "The operation was attempted past a valid range of some kind -- e.g., reading past "
                "the end of a file";
      case RpcStatusCode::Unimplemented:
         return "The operation is not implemented or is not supported/enabled in this service";
      case RpcStatusCode::Internal:
         return "Internal error; some invariants expected by the underlying system have been "
                "broken";
      case RpcStatusCode::Unavailable: return "The service is currently unavailable";
      case RpcStatusCode::DataLoss: return "Unrecoverable data loss or corruption";
      case RpcStatusCode::Unauthenticated: return "Unauthenticated access";
      case RpcStatusCode::Unspecified: return "Some unspecified error";
      }
      return "(unknown error)";
   }
};

} // namespace sgrpc
