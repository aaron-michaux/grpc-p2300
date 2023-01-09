
#pragma once

#include "sgrpc/sgrpc.hpp"

namespace Greeting
{

class Server final
{
 public:
   //@{ Construction/Destruction
   /**
    * @param execution_context The ExecutionContext that processes events (after `start()`).
    * @param number_work_queues The number of work (server-completion) queues to create.
    * @param port The port to listen on; if zero is passed, then a port is selected.
    *
    * Lifecycle:
    * + The Server object must live until `execution_context` has stopped.
    */
   explicit Server(sgrpc::ExecutionContext& execution_context,
                   uint32_t number_work_queues = 1,
                   uint16_t port               = 0);
   ~Server();
   //@}

   //@{ Action!
   /**
    * If constructed with `port == 0`, then a port is selected.
    * @return The port listening on.
    */
   uint16_t start(std::shared_ptr<grpc::ServerCredentials> credentials
                  = grpc::InsecureServerCredentials());
   //@}

 private:
   /**
    * Use a private implementation to type-erase the server from the underlying grpc types.
    */
   struct Impl_;
   std::unique_ptr<Impl_> impl_;
};

void run_server();

} // namespace Greeting
