
#pragma once

#include "detail/base_inc.hpp"

#include <grpcpp/grpcpp.h>

namespace sgrpc
{

class ServerContainerInterface
{
 public:
   virtual ~ServerContainerInterface() = default;

   /**
    * A server needs some way of sharing its completion queues with the execution-context,
    * so that the execution context and process the events.
    */
   virtual std::vector<std::unique_ptr<grpc::ServerCompletionQueue>>& get_work_queues() = 0;
};

} // namespace sgrpc
