
#pragma once

#include "sgrpc/sgrpc.hpp"

#include <protos/helloworld.grpc.pb.h>

namespace Greeting
{

/**
 * The logic of the server
 */
class Server final
{
 public:
   helloworld::HelloReply say_hello(const grpc::ServerContext& server_context,
                                    const helloworld::HelloRequest& request)
   {
      helloworld::HelloReply reply;
      reply.set_message(fmt::format("Say, request='{}'", request.name()));
      return reply;
   }
};

/**
 * A handle to a running server listening on a specific port
 */
class ServerHandle
{
 public:
   ~ServerHandle() = default;              //!< The server shuts down when instance destructs
   uint16_t port() const { return port_; } //!< The port the server is listening on

   /**
    * Creates a new server (instance)
    *
    * @param number_server_work_queues The number of work (server-completion) queues to create.
    * @param port The port to listen on; if zero is passed, then a port is selected.
    * @param credentials The grpc credentials, if any.
    * @return The port listening on.
    */
   static ServerHandle build(sgrpc::ExecutionContext& execution_context,
                             std::shared_ptr<Server> server,
                             uint32_t number_server_work_queues = 1,
                             uint16_t port                      = 0,
                             std::shared_ptr<grpc::ServerCredentials> credentials
                             = grpc::InsecureServerCredentials()) noexcept(false);

 private:
   std::shared_ptr<Server> server_;
   uint16_t port_{0};
};

} // namespace Greeting
