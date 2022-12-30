
#include "stdinc.hpp"

#include "greeting-grpc/greeting-client.h"
#include "greeting-grpc/greeting-server.h"

#include <thread>

// -------------------------------------------------------------------------------------------------
/**
 * concept scheduler:
 *    schedule(scheduler) -> sender
 *
 * concept sender:
 *    connect(sender, receiver) -> operation_state
 *
 * concept receiver:
 *    set_value(reciever, Values...) -> void
 *    set_value(receiver, Error...) -> void
 *    set_stopped(receiver) -> void
 *
 * concept operation_state:
 *    start(operation_state) -> void
 */

int main(int, char**) {

  std::thread server_thread{GreetingServer::run_server};
  GreetingClient::run_client();
  server_thread.join();

  return EXIT_SUCCESS;
}
