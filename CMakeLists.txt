cmake_minimum_required(VERSION 3.1...3.10)

project(bcx_mustex VERSION 1.0 LANGUAGES CXX)

set(BCX_MUSTEX_TARGET_NAME "bcx_mustex")

option(MUSTEX_BUILD_TESTS  "Enable build of unit tests." OFF)
option(MUSTEX_TESTS_CXX_20 "Use c++ standard 20 and above for tests." ON)
option(MUSTEX_TESTS_CXX_14 "Use c++ standard 17 for tests." OFF)
option(MUSTEX_TESTS_CXX_17 "Use c++ standard 14 for tests." OFF)

add_library(${BCX_MUSTEX_TARGET_NAME} INTERFACE)
target_include_directories(${BCX_MUSTEX_TARGET_NAME} INTERFACE include)

if(MUSTEX_BUILD_TESTS)
    # Setup unit tests with catch 2
    Include(FetchContent)

    set(TESTS_TARGET mustex_tests)

    add_executable(${TESTS_TARGET} tests/tests.cpp)
    target_link_libraries(${TESTS_TARGET} PRIVATE ${BCX_MUSTEX_TARGET_NAME})

    if(MUSTEX_TESTS_CXX_20)
        set_property(TARGET ${TESTS_TARGET} PROPERTY CXX_STANDARD 20)
    elseif(MUSTEX_TESTS_CXX_17)
        set_property(TARGET ${TESTS_TARGET} PROPERTY CXX_STANDARD 17)
    elseif(MUSTEX_TESTS_CXX_14)
        set_property(TARGET ${TESTS_TARGET} PROPERTY CXX_STANDARD 14)
    else()
        set_property(TARGET ${TESTS_TARGET} PROPERTY CXX_STANDARD 11)
    endif()

    # /Zc:__cplusplus is required to make __cplusplus accurate
    # /Zc:__cplusplus is available starting with Visual Studio 2017 version 15.7
    # (according to https://learn.microsoft.com/en-us/cpp/build/reference/zc-cplusplus)
    # That version is equivalent to _MSC_VER==1914
    # (according to https://learn.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2019)
    # CMake's ${MSVC_VERSION} is equivalent to _MSC_VER
    # (according to https://cmake.org/cmake/help/latest/variable/MSVC_VERSION.html#variable:MSVC_VERSION)
    if ((MSVC) AND (MSVC_VERSION GREATER_EQUAL 1914))
        target_compile_options(${TESTS_TARGET} PUBLIC "/Zc:__cplusplus")
    endif()

    if(MSVC)
        target_compile_options(${TESTS_TARGET} PRIVATE /W4 /WX)
    else()
        target_compile_options(${TESTS_TARGET} PRIVATE -Wall -Wextra -Wpedantic -Werror)
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