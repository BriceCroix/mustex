cmake_minimum_required(VERSION 3.1)

project(bcx_mustex VERSION 1.0 LANGUAGES CXX)

set(BCX_MUSTEX_TARGET_NAME "bcx_mustex")

option(MUSTEX_CXX_STANDARD_17 "Use c++ standard 17 and above." ON)
option(MUSTEX_BUILD_TESTS     "Enable build of unit tests." OFF)

add_library(${BCX_MUSTEX_TARGET_NAME} INTERFACE)
target_include_directories(${BCX_MUSTEX_TARGET_NAME} INTERFACE include)

if(MUSTEX_CXX_STANDARD_17)
    target_compile_features(${BCX_MUSTEX_TARGET_NAME} INTERFACE cxx_std_17)
else()
    target_compile_features(${BCX_MUSTEX_TARGET_NAME} INTERFACE cxx_std_11)
endif(MUSTEX_CXX_STANDARD_17)


if(MUSTEX_BUILD_TESTS)
    Include(FetchContent)

    set(TESTS_TARGET mustex_tests)

    add_executable(${TESTS_TARGET} tests/tests.cpp)
    target_link_libraries(${TESTS_TARGET} PRIVATE ${BCX_MUSTEX_TARGET_NAME})

    if(MUSTEX_CXX_STANDARD_17)
        target_compile_features(${TESTS_TARGET} INTERFACE cxx_std_17)
    else()
        target_compile_features(${TESTS_TARGET} INTERFACE cxx_std_11)
    endif(MUSTEX_CXX_STANDARD_17)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
    target_link_libraries(${TESTS_TARGET} PRIVATE Catch2::Catch2WithMain)

    find_package(Threads REQUIRED)
    target_link_libraries(${TESTS_TARGET} PRIVATE Threads::Threads)
endif(MUSTEX_BUILD_TESTS)