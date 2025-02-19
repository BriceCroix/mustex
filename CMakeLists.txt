cmake_minimum_required(VERSION 3.1)

project(bcx_mustex VERSION 1.0 LANGUAGES CXX)

set(BCX_MUSTEX_TARGET_NAME "bcx_mustex")

option(MUSTEX_BUILD_TESTS  "Enable build of unit tests." OFF)
option(MUSTEX_TESTS_CXX_20 "Use c++ standard 20 and above for tests." ON)
option(MUSTEX_TESTS_CXX_17 "Use c++ standard 17 for tests." OFF)

add_library(${BCX_MUSTEX_TARGET_NAME} INTERFACE)
target_include_directories(${BCX_MUSTEX_TARGET_NAME} INTERFACE include)

if(MUSTEX_BUILD_TESTS)
    # Setup unit tests with catch 2
    Include(FetchContent)

    set(TESTS_TARGET mustex_tests)

    add_executable(${TESTS_TARGET} tests/tests.cpp)
    target_link_libraries(${TESTS_TARGET} PRIVATE ${BCX_MUSTEX_TARGET_NAME})

    if(MUSTEX_TESTS_CXX_20)
        target_compile_features(${TESTS_TARGET} PUBLIC cxx_std_20)
    elseif(MUSTEX_TESTS_CXX_17)
        target_compile_features(${TESTS_TARGET} PUBLIC cxx_std_17)
    else()
        target_compile_features(${TESTS_TARGET} PUBLIC cxx_std_11)
    endif()

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
    target_link_libraries(${TESTS_TARGET} PRIVATE Catch2::Catch2WithMain)

    find_package(Threads REQUIRED)
    target_link_libraries(${TESTS_TARGET} PRIVATE Threads::Threads)

    # Register catch tests to CTest
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)
    enable_testing()
    catch_discover_tests(${TESTS_TARGET})
endif(MUSTEX_BUILD_TESTS)