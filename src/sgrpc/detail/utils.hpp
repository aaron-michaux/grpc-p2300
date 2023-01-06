
#pragma once

#include "sgrpc/rpc_status_code.hpp"

#include <grpcpp/completion_queue.h>
#include <grpcpp/support/status.h>

#include <chrono>

namespace sgrpc::detail
{
inline gpr_timespec duration_to_grp_timespec(std::chrono::nanoseconds delta)
{
   gpr_timespec timeout_gpr;
   const auto seconds     = std::chrono::duration_cast<std::chrono::seconds>(delta);
   const auto nanos       = delta - seconds;
   timeout_gpr.clock_type = GPR_CLOCK_MONOTONIC;
   timeout_gpr.tv_sec     = seconds.count();
   timeout_gpr.tv_nsec    = nanos.count();
   return timeout_gpr;
}

inline RpcStatusCode to_rpc_status_code(grpc::StatusCode code)
{
   switch(code) {
   case grpc::StatusCode::OK: return RpcStatusCode::Ok;
   case grpc::StatusCode::CANCELLED: return RpcStatusCode::Cancelled;
   case grpc::StatusCode::UNKNOWN: return RpcStatusCode::Unknown;
   case grpc::StatusCode::INVALID_ARGUMENT: return RpcStatusCode::InvalidArgument;
   case grpc::StatusCode::DEADLINE_EXCEEDED: return RpcStatusCode::DeadlineExceeded;
   case grpc::StatusCode::NOT_FOUND: return RpcStatusCode::NotFound;
   case grpc::StatusCode::ALREADY_EXISTS: return RpcStatusCode::AlreadyExists;
   case grpc::StatusCode::PERMISSION_DENIED: return RpcStatusCode::PermissionDenied;
   case grpc::StatusCode::UNAUTHENTICATED: return RpcStatusCode::Unauthenticated;
   case grpc::StatusCode::RESOURCE_EXHAUSTED: return RpcStatusCode::ResourceExhausted;
   case grpc::StatusCode::FAILED_PRECONDITION: return RpcStatusCode::FailedPrecondition;
   case grpc::StatusCode::ABORTED: return RpcStatusCode::Aborted;
   case grpc::StatusCode::OUT_OF_RANGE: return RpcStatusCode::OutOfRange;
   case grpc::StatusCode::UNIMPLEMENTED: return RpcStatusCode::Unimplemented;
   case grpc::StatusCode::INTERNAL: return RpcStatusCode::Internal;
   case grpc::StatusCode::UNAVAILABLE: return RpcStatusCode::Unavailable;
   case grpc::StatusCode::DATA_LOSS: return RpcStatusCode::DataLoss;
   case grpc::StatusCode::DO_NOT_USE: return RpcStatusCode::Unspecified;
   }
   return RpcStatusCode::Unspecified;
}

} // namespace sgrpc::detail
