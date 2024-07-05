include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

add_executable(
  mpmc_tests
  mpmc/example_test.cpp
)

target_link_libraries(
  mpmc_tests
  GTest::gtest_main
)

target_include_directories(mpmc_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_executable(
  spsc_tests
  spsc/queue_test.cpp
)

target_link_libraries(
  spsc_tests
  GTest::gtest_main
)

target_include_directories(mpmc_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)

include(GoogleTest)
gtest_discover_tests(spsc_tests)
gtest_discover_tests(mpmc_tests)
