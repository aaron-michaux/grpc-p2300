
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

inline grpc::StatusCode to_grpc_status_code(RpcStatusCode code)
{
   switch(code) {
   case RpcStatusCode::Ok: return grpc::StatusCode::OK;
   case RpcStatusCode::Cancelled: return grpc::StatusCode::CANCELLED;
   case RpcStatusCode::Unknown: return grpc::StatusCode::UNKNOWN;
   case RpcStatusCode::InvalidArgument: return grpc::StatusCode::INVALID_ARGUMENT;
   case RpcStatusCode::DeadlineExceeded: return grpc::StatusCode::DEADLINE_EXCEEDED;
   case RpcStatusCode::NotFound: return grpc::StatusCode::NOT_FOUND;
   case RpcStatusCode::AlreadyExists: return grpc::StatusCode::ALREADY_EXISTS;
   case RpcStatusCode::PermissionDenied: return grpc::StatusCode::PERMISSION_DENIED;
   case RpcStatusCode::Unauthenticated: return grpc::StatusCode::UNAUTHENTICATED;
   case RpcStatusCode::ResourceExhausted: return grpc::StatusCode::RESOURCE_EXHAUSTED;
   case RpcStatusCode::FailedPrecondition: return grpc::StatusCode::FAILED_PRECONDITION;
   case RpcStatusCode::Aborted: return grpc::StatusCode::ABORTED;
   case RpcStatusCode::OutOfRange: return grpc::StatusCode::OUT_OF_RANGE;
   case RpcStatusCode::Unimplemented: return grpc::StatusCode::UNIMPLEMENTED;
   case RpcStatusCode::Internal: return grpc::StatusCode::INTERNAL;
   case RpcStatusCode::Unavailable: return grpc::StatusCode::UNAVAILABLE;
   case RpcStatusCode::DataLoss: return grpc::StatusCode::DATA_LOSS;
   case RpcStatusCode::Unspecified: return grpc::StatusCode::DO_NOT_USE;
   }
   return grpc::StatusCode::UNKNOWN;
}

} // namespace sgrpc::detail
