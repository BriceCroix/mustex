#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <future>
#include <mustex/mustex.hpp>
#include <thread>

using namespace bcx;

class MyClass
{
public:
    MyClass(int data)
        : m_data{data} {}
    int do_things() const { return m_data * 2; };
    int do_things_mut()
    {
        m_data *= 3;
        return m_data;
    };

private:
    int m_data;
};

TEST_CASE("Construct mustex", "[mustex]")
{
    REQUIRE_NOTHROW(Mustex<int>(int{}));
}

#if __cplusplus >= 201703L
TEST_CASE("Lock mustex", "[mustex]")
{
    Mustex m(int{42});
    auto handle = m.lock();
    REQUIRE(*handle == 42);
}
#endif // #if __cplusplus >= 201703L

TEST_CASE("Lock mustex mutably", "[mustex]")
{
    Mustex<int> m(int{42});
    auto handle = m.lock_mut();
    REQUIRE(*handle == 42);
    *handle = 8;
    REQUIRE(*handle == 8);
}

#if __cplusplus >= 201703L
TEST_CASE("Access mustex readonly", "[mustex]")
{
    Mustex m(MyClass(111));
    auto handle = m.lock();
    REQUIRE(handle->do_things() == 222);
}
#endif // #if __cplusplus >= 201703L

TEST_CASE("Access mustex mutably", "[mustex]")
{
    Mustex<MyClass> m(MyClass(111));
    auto handle = m.lock_mut();
    REQUIRE(handle->do_things() == 222);
    REQUIRE(handle->do_things_mut() == 333);
    REQUIRE(handle->do_things() == 666);
}

#if __cplusplus >= 201703L
TEST_CASE("Lock mustex readonly twice", "[mustex]")
{
    Mustex m(int{42});

    auto tic = std::chrono::system_clock::now();

    auto future = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    auto future2 = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::system_clock::now();
    REQUIRE(tac - tic < std::chrono::milliseconds(150));
}

TEST_CASE("Lock mustex mutably while locked readonly", "[mustex]")
{
    Mustex m(int{42});

    auto tic = std::chrono::system_clock::now();

    auto future = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    auto future2 = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock_mut();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::system_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
}

TEST_CASE("Lock mustex readonly while locked mutably", "[mustex]")
{
    Mustex m(int{42});

    auto tic = std::chrono::system_clock::now();

    auto future = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock_mut();
            REQUIRE(*handle == 42);
            *handle = 15;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    auto future2 = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 15);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::system_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
}
#endif // #if __cplusplus >= 201703L

TEST_CASE("Lock mustex mutably while locked mutably", "[mustex]")
{
    Mustex<int> m(int{42});

    auto tic = std::chrono::system_clock::now();

    auto future = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock_mut();
            REQUIRE(*handle == 42);
            *handle = 15;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    auto future2 = std::async(
        std::launch::async,
        [&m]
        {
            auto handle = m.lock_mut();
            REQUIRE(*handle == 15);
            *handle = 12;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::system_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
}

TEST_CASE("Transfer handle ownership mutably", "[mustex]")
{
    Mustex<int> m(int{42});

    auto handle = m.lock_mut();
    MustexHandle handle2(std::move(handle));

    REQUIRE(*handle2 == 42);
}

#if __cplusplus >= 201703L
TEST_CASE("Transfer handle ownership", "[mustex]")
{
    Mustex<int> m(int{42});

    auto handle = m.lock();
    MustexHandle handle2(std::move(handle));

    REQUIRE(*handle2 == 42);
}
#endif // #if __cplusplus >= 201703L