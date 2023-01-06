
#pragma once

namespace sgrpc
{
enum class RpcStatusCode : int {
   Ok = 0,
   Cancelled,
   Unknown,
   InvalidArgument,
   DeadlineExceeded,
   NotFound,
   AlreadyExists,
   PermissionDenied,
   ResourceExhausted,
   FailedPrecondition,
   Aborted,
   OutOfRange,
   Unimplemented,
   Internal,
   Unavailable,
   DataLoss,
   Unauthenticated,
   LogicError,
   Unspecified
};
}
