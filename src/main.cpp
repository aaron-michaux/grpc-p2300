
#include "stdinc.hpp"

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

int main(int, char**) { return EXIT_SUCCESS; }
