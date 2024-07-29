include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git GIT_TAG v1.15.0)
FetchContent_MakeAvailable(googletest)

add_executable(mpmc_tests mpmc/queue_test.cpp)
target_include_directories(mpmc_tests PRIVATE ${CMAKE_SOURCE_DIR}/include/blockbuster)
target_link_libraries(mpmc_tests PRIVATE GTest::gtest_main)

add_executable(spsc_tests spsc/queue_test.cpp)
target_include_directories(spsc_tests PRIVATE ${CMAKE_SOURCE_DIR}/include/blockbuster)
target_link_libraries(spsc_tests PRIVATE GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(mpmc_tests)
gtest_discover_tests(spsc_tests)
