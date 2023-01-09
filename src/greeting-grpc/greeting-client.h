
#pragma once

#include "sgrpc/sgrpc.hpp"

namespace Greeting
{

/**
 * The client side of the
 */
class Client final
{
 public:
   //@{ Construction/Destruction
   /**
    * @param context The execution engine to process asynchronous events
    * @param channel The channel through which to connect to the server
    */
   explicit Client(sgrpc::ExecutionContext& context, std::shared_ptr<grpc::Channel> channel);
   ~Client();
   //@}

   /**
    * Returns a sender<string> (like a future) result
    */
   sgrpc::RpcSender<std::string> say_hello(std::string user);

 private:
   /**
    * Use a private implementation to type-erase the client from the underlying grpc types.
    */
   struct Impl_;
   std::unique_ptr<Impl_> impl_;
};

} // namespace Greeting
