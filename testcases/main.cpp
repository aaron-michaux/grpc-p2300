
// #define CATCH_CONFIG_RUNNER

// #include <catch2/catch.hpp>

// int main(int argc, char* argv[]) {
//   int result = Catch::Session().run(argc, argv);
//   return result;
// }

#include <gtest/gtest.h>

TEST(Something, Foo)
{
   int x = 0;
   ASSERT_EQ(x, 0);
}
