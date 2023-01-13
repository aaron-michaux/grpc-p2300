
# TODOs - roughly speaking
 * [DONE] port reservation to use multiple
 * [DONE] ./run.sh check-all to build test/examples/tsan/asan/etc.
 * [DONE] tsan build of grpc???
 * [DONE] remove grpc status code to firewall off grpc code from everything else.
 * [DONE] The server::impl should tie its lifecycle to the execution context
 * What about different types of RPCs for grpc? (streaming???)
 * Implement Senders for async server
 * Unify client and server senders... so that it all looks the same. (Can it be done?)
 * error/cancellation/chaining propagation, with void
   - How do types propagate?
 * Integrate in logging framework. (dependency injection)
 * Look at IoC container (https://github.com/ybainier/Hypodermic)
 * gtest testcases + coverage
 * doc folder for tutorial... see rust example?
   - examples
 * doxygen... detailed... integrates by-hand docs??
 * plantuml integration with docs
