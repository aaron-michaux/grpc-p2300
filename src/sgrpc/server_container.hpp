
#pragma once

#include "execution_context.hpp"
#include "scheduler.hpp"

#include <fmt/format.h>

namespace sgrpc
{

template<typename Service, typename Server> class ServerContainer : public ServerContainerInterface
{
 public:
   static std::shared_ptr<ServerContainer> make(
       ExecutionContext& execution_context,
       std::shared_ptr<Server> server,
       std::function<void(Server&, Service&, Scheduler, grpc::ServerCompletionQueue& cq)> wire_rpcs,
       uint32_t number_work_queues = 1,
       uint16_t port               = 0,
       std::shared_ptr<grpc::ServerCredentials> credentials
       = grpc::InsecureServerCredentials()) noexcept(false)
   {
      auto container = std::make_shared<ServerContainer>();
      container->init(execution_context,
                      std::move(server),
                      std::move(wire_rpcs),
                      number_work_queues,
                      port,
                      std::move(credentials));

      // Attach to the execution context... so that the container lives until then
      // execution context is stopped.
      execution_context.attach_server(container);

      return container;
   }

   uint16_t port() const { return port_; }

   std::vector<std::unique_ptr<grpc::ServerCompletionQueue>>& get_work_queues() override
   {
      return cqs_;
   }

 private:
   void init(
       ExecutionContext& execution_context,
       std::shared_ptr<Server> server,
       std::function<void(Server&, Service&, Scheduler, grpc::ServerCompletionQueue& cq)> wire_rpcs,
       uint32_t number_work_queues,
       uint16_t port,
       std::shared_ptr<grpc::ServerCredentials> credentials)
   {
      // if port is zero, then what?
      if(number_work_queues == 0) throw std::invalid_argument{"requires at least 1 work queue"};

      // Take control of the server instance
      server_ = std::move(server);

      grpc::ServerBuilder builder;

      // Set the listening port for this server
      std::string server_address(fmt::format("0.0.0.0:{}", port));
      int selected_port = static_cast<int>(port);
      builder.AddListeningPort(server_address, std::move(credentials), &selected_port);

      // The instance through which RPCs are handled
      builder.RegisterService(&service_);

      // Create the work queues
      cqs_.reserve(number_work_queues);
      for(auto i = 0u; i < number_work_queues; ++i) cqs_.push_back(builder.AddCompletionQueue());

      // Assemble the server.
      grpc_server_ = builder.BuildAndStart();

      // What port did we get?
      if(port_ == 0) port_ = static_cast<uint16_t>(selected_port);
      assert(port_ == static_cast<uint16_t>(selected_port));

      // Register RPC callbacks onto the server completion queues
      for(auto& cq : cqs_) wire_rpcs(*server_, service_, sgrpc::Scheduler{execution_context}, *cq);
   }

   Service service_;                           //!< grpc generated code
   std::unique_ptr<grpc::Server> grpc_server_; //!< The grpc server
   std::shared_ptr<Server> server_;            //!< The application code server
   std::vector<std::unique_ptr<grpc::ServerCompletionQueue>> cqs_;
   uint16_t port_{0};
};

} // namespace sgrpc
