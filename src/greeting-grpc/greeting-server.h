
#pragma once

#include "sgrpc/sgrpc.hpp"

namespace Greeting
{

class Server final
{
 private:
   Server();

 public:
   //@{ Construction/Destruction
   ~Server();

   /**
    * Creates a new server (instance)
    *
    * @param number_server_work_queues The number of work (server-completion) queues to create.
    * @param port The port to listen on; if zero is passed, then a port is selected.
    * @param credentials The grpc credentials, if any.
    * @return The port listening on.
    */
   Server make(sgrpc::ExecutionContext& execution_context,
               uint32_t number_server_work_queues = 1,
               uint16_t port                      = 0,
               std::shared_ptr<grpc::ServerCredentials> credentials
               = grpc::InsecureServerCredentials()) noexcept(false);
   //@}

   uint16_t get_port() const; //!< The port this server is listening on

 private:
   class Impl;
   std::shared_ptr<Impl> impl_; // A copy of `impl` is given to execution_context
};

void run_server();

} // namespace Greeting
