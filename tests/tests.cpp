#if CATCH2_VERSION == 2
#    define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#    include <catch2/catch.hpp>
#else
#    include <catch2/catch_test_macros.hpp>
#endif

#include <chrono>
#include <future>
#include <mustex/mustex.hpp>
#include <thread>

using namespace bcx;

class BasicLockable
{
public:
    void lock() { m.lock(); }
    void unlock() { m.unlock(); }

private:
    std::mutex m;
};

class Lockable
{
public:
    void lock() { m.lock(); }
    bool try_lock() { return m.try_lock(); }
    void unlock() { m.unlock(); }

private:
    std::mutex m;
};

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

    int get_data() { return m_data; }

private:
    int m_data;
};

TEST_CASE("Construct mustex", "[mustex]")
{
    REQUIRE_NOTHROW(Mustex<int>(int{}));
}

TEST_CASE("Lock mustex", "[mustex]")
{
    Mustex<int> m(42);
    auto handle = m.lock();
    REQUIRE(*handle == 42);
}

TEST_CASE("Lock mustex mutably", "[mustex]")
{
    Mustex<int> m(42);
    auto handle = m.lock_mut();
    REQUIRE(*handle == 42);
    *handle = 8;
    REQUIRE(*handle == 8);
}

TEST_CASE("Access mustex readonly", "[mustex]")
{
    Mustex<MyClass> m(MyClass(111));
    auto handle = m.lock();
    REQUIRE(handle->do_things() == 222);
}

TEST_CASE("Access mustex mutably", "[mustex]")
{
    Mustex<MyClass> m(MyClass(111));
    auto handle = m.lock_mut();
    REQUIRE(handle->do_things() == 222);
    REQUIRE(handle->do_things_mut() == 333);
    REQUIRE(handle->do_things() == 666);
}

#ifdef _MUSTEX_HAS_SHARED_MUTEX
// Without shared mutex there can only be one reader at a time and this code would deadlock.
TEST_CASE("Lock mustex readonly without deadlock", "[mustex]")
{
    bcx::Mustex<std::string> name("Batman");
    auto handle = name.lock();
    auto future = std::async(
        [&name]
        {
            auto handle = name.lock();
            volatile std::string copy = *handle;
        }
    );
    volatile std::string copy = *handle;
    future.wait();
    // The simple fact that this test ends is a proof of simultaneous read-access.
    REQUIRE(true);
}
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX

TEST_CASE("Lock mustex readonly twice", "[mustex]")
{
    Mustex<int> m(42);

    auto tic = std::chrono::high_resolution_clock::now();

    auto future = std::async(
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    auto future2 = std::async(
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::high_resolution_clock::now();
#ifdef _MUSTEX_HAS_SHARED_MUTEX
    REQUIRE(tac - tic < std::chrono::milliseconds(150));
#else // #ifdef _MUSTEX_HAS_SHARED_MUTEX
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX
}

TEST_CASE("Lock mustex mutably while locked readonly", "[mustex]")
{
    Mustex<int> m(42);

    auto tic = std::chrono::high_resolution_clock::now();

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock();
            started = true;
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    auto future2 = std::async(
        [&m]
        {
            auto handle = m.lock_mut();
            REQUIRE(*handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
}

TEST_CASE("Lock mustex readonly while locked mutably", "[mustex]")
{
    Mustex<int> m(42);

    auto tic = std::chrono::high_resolution_clock::now();

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock_mut();
            started = true;
            REQUIRE(*handle == 42);
            *handle = 15;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    auto future2 = std::async(
        [&m]
        {
            auto handle = m.lock();
            REQUIRE(*handle == 15);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    future.wait();
    future2.wait();

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
}

TEST_CASE("Lock mustex mutably while locked mutably", "[mustex]")
{
    Mustex<int> m(42);

    auto tic = std::chrono::high_resolution_clock::now();

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock_mut();
            started = true;
            REQUIRE(*handle == 42);
            *handle = 15;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    auto future2 = std::async(
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

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(200));
}

TEST_CASE("Try lock mutably", "[mustex]")
{
    Mustex<int> m(42);
    std::atomic<bool> started{false};

    auto future = std::async(
        [&m, &started]
        {
            auto opt_handle = m.try_lock_mut();
            started = true;
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
            **opt_handle = 45;
            REQUIRE(**opt_handle == 45);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    while (!started)
        ;

    auto opt_handle2 = m.lock_mut(std::try_to_lock);
    REQUIRE_FALSE(opt_handle2);
    future.wait();
}

TEST_CASE("Try lock", "[mustex]")
{
    Mustex<int> m(42);
    std::atomic<bool> started{false};

    auto future = std::async(
        [&m, &started]
        {
            auto opt_handle = m.try_lock();
            started = true;
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    while (!started)
        ;

    auto future2 = std::async(
        [&m]
        {
            auto opt_handle = m.lock(std::try_to_lock);
#ifdef _MUSTEX_HAS_SHARED_MUTEX
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
#else
            REQUIRE_FALSE(opt_handle);
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX
        }
    );

    auto opt_handle = m.try_lock_mut();
    REQUIRE_FALSE(opt_handle);
    future.wait();
    future2.wait();
}

TEST_CASE("Try lock mutably for", "[mustex]")
{
    Mustex<int> m(42);
    std::atomic<bool> started{false};

    auto future = std::async(
        [&m, &started]
        {
            auto opt_handle = m.try_lock_mut_for(std::chrono::nanoseconds(1));
            started = true;
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
            **opt_handle = 45;
            REQUIRE(**opt_handle == 45);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    while (!started)
        ;

    {
        auto opt_handle = m.try_lock_mut_for(std::chrono::milliseconds(20));
        REQUIRE_FALSE(opt_handle);
    }
    {
        auto opt_handle = m.try_lock_mut_for(std::chrono::milliseconds(100 - 20));
        REQUIRE(opt_handle);
        REQUIRE(**opt_handle == 45);
    }
    future.wait();
}

TEST_CASE("Try lock for", "[mustex]")
{
    Mustex<int> m(42);
    std::atomic<bool> started{false};

    auto future = std::async(
        [&m, &started]
        {
            auto opt_handle = m.try_lock_for(std::chrono::nanoseconds(1));
            started = true;
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    while (!started)
        ;

    {
        auto opt_handle = m.try_lock_for(std::chrono::nanoseconds(1));
#ifdef _MUSTEX_HAS_SHARED_MUTEX
        REQUIRE(opt_handle);
        REQUIRE(**opt_handle == 42);
#else
        REQUIRE_FALSE(opt_handle);
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX
    }
    {
        auto opt_handle = m.try_lock_mut_for(std::chrono::milliseconds(20));
        REQUIRE_FALSE(opt_handle);
    }
    {
        auto opt_handle = m.try_lock_mut_for(std::chrono::milliseconds(100 - 20));
        REQUIRE(opt_handle);
        REQUIRE(**opt_handle == 42);
    }
    future.wait();
}

TEST_CASE("Try lock mutably until", "[mustex]")
{
    Mustex<int> m(42);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto opt_handle = m.try_lock_mut_until(std::chrono::high_resolution_clock::time_point::max());
            started = true;
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
            **opt_handle = 45;
            REQUIRE(**opt_handle == 45);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    while (!started)
        ;
    const auto start_tp = std::chrono::high_resolution_clock::now();

    {
        auto opt_handle = m.try_lock_mut_until(start_tp + std::chrono::milliseconds(20));
        REQUIRE_FALSE(opt_handle);
    }
    {
        auto opt_handle = m.try_lock_mut_until(start_tp + std::chrono::milliseconds(110));
        REQUIRE(opt_handle);
        REQUIRE(**opt_handle == 45);
    }
    future.wait();
}

TEST_CASE("Try lock until", "[mustex]")
{
    Mustex<int> m(42);
    std::atomic<bool> started{false};

    auto future = std::async(
        [&m, &started]
        {
            auto opt_handle = m.try_lock_until(std::chrono::high_resolution_clock::time_point::max());
            started = true;
            REQUIRE(opt_handle);
            REQUIRE(**opt_handle == 42);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );

    while (!started)
        ;
    const auto start_tp = std::chrono::high_resolution_clock::now();

    {
        auto opt_handle = m.try_lock_until(start_tp + std::chrono::nanoseconds(1));
#ifdef _MUSTEX_HAS_SHARED_MUTEX
        REQUIRE(opt_handle);
        REQUIRE(**opt_handle == 42);
#else
        REQUIRE_FALSE(opt_handle);
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX
    }
    {
        auto opt_handle = m.try_lock_mut_until(start_tp + std::chrono::milliseconds(20));
        REQUIRE_FALSE(opt_handle);
    }
    {
        auto opt_handle = m.try_lock_mut_until(start_tp + std::chrono::milliseconds(110));
        REQUIRE(opt_handle);
        REQUIRE(**opt_handle == 42);
    }
    future.wait();
}

// TODO try_lock for/until

TEST_CASE("Move handle mutably (construct)", "[mustex]")
{
    Mustex<int> m(42);

    auto handle = m.lock_mut();
    decltype(handle) handle2(std::move(handle));

    REQUIRE(*handle2 == 42);
}

TEST_CASE("Move handle (construct)", "[mustex]")
{
    Mustex<int> m(42);

    auto handle = m.lock();
    decltype(handle) handle2(std::move(handle));

    REQUIRE(*handle2 == 42);
}

TEST_CASE("Move handle mutably (assign)", "[mustex]")
{
    Mustex<int> a(1);
    Mustex<int> b(2);

    {
        auto ha = a.lock_mut();
        auto hb = b.lock_mut();
        hb = std::move(ha);
        *hb = 3;
    }

    REQUIRE(*a.lock() == 3);
    REQUIRE(*b.lock() == 2);
}

TEST_CASE("Move handle (assign)", "[mustex]")
{
    Mustex<int> a(1);
    Mustex<int> b(2);

    {
        auto ha = a.lock();
        auto hb = b.lock();
        hb = std::move(ha);
    }

    REQUIRE(*a.lock() == 1);
    REQUIRE(*b.lock() == 2);
}

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Copy mustex unused (construct)", "[mustex]")
{
    Mustex<int> m(42);

    auto tic = std::chrono::high_resolution_clock::now();
    decltype(m) m2(m);
    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic < std::chrono::milliseconds(10));

    REQUIRE(*m.lock() == 42);
    REQUIRE(*m2.lock() == 42);
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Copy mustex used mutably (construct)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock_mut();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    decltype(m) m2(m);

    REQUIRE(*m2.lock() == 42);

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Copy mustex used readonly (construct)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    decltype(m) m2(m);

    REQUIRE(*m2.lock() == 42);

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic < std::chrono::milliseconds(80));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Copy mustex unused (assign)", "[mustex]")
{
    Mustex<int> m1(42);
    Mustex<int> m2(3);

    auto tic = std::chrono::high_resolution_clock::now();
    m2 = m1;
    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic < std::chrono::milliseconds(10));

    REQUIRE(*m1.lock() == 42);
    REQUIRE(*m2.lock() == 42);
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Copy mustex used mutably (assign)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);
    Mustex<int> m2(2);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock_mut();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    m2 = m;

    auto tac = std::chrono::high_resolution_clock::now();

    REQUIRE(*m2.lock() == 42);
    REQUIRE(*m.lock() == 42);
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Copy mustex used readonly (assign)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);
    Mustex<int> m2(2);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    m2 = m;

    auto tac = std::chrono::high_resolution_clock::now();

    REQUIRE(*m2.lock() == 42);
    REQUIRE(*m.lock() == 42);
    REQUIRE(tac - tic < std::chrono::milliseconds(80));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Move mustex unused (construct)", "[mustex]")
{
    Mustex<int> m(42);

    auto tic = std::chrono::high_resolution_clock::now();
    decltype(m) m2(std::move(m));
    auto tac = std::chrono::high_resolution_clock::now();

    REQUIRE(tac - tic < std::chrono::milliseconds(10));
    REQUIRE(*m2.lock() == 42);
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Move mustex used mutably (construct)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock_mut();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    decltype(m) m2(std::move(m));

    REQUIRE(*m2.lock() == 42);

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Move mustex used readonly (construct)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    decltype(m) m2(std::move(m));

    REQUIRE(*m2.lock() == 42);

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Move mustex unused (assign)", "[mustex]")
{
    Mustex<int> m(42);
    decltype(m) m2(5);

    auto tic = std::chrono::high_resolution_clock::now();
    m2 = std::move(m);
    auto tac = std::chrono::high_resolution_clock::now();

    REQUIRE(tac - tic < std::chrono::milliseconds(10));
    REQUIRE(*m2.lock() == 42);
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Move mustex used mutably (assign)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);
    Mustex<int> m2(2);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock_mut();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    m2 = std::move(m);

    auto tac = std::chrono::high_resolution_clock::now();

    REQUIRE(*m2.lock() == 42);
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_CONCEPTS
TEST_CASE("Move mustex used readonly (assign)", "[mustex]")
{
    auto tic = std::chrono::high_resolution_clock::now();

    Mustex<int> m(42);
    Mustex<int> m2(2);

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            auto handle = m.lock();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    m2 = std::move(m);

    auto tac = std::chrono::high_resolution_clock::now();

    REQUIRE(*m2.lock() == 42);
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

TEST_CASE("Synchronous lock unused", "[mustex]")
{
    Mustex<MyClass> shared1(1);
    Mustex<float> shared2(2.f);
    std::mutex m;

    auto locks = lock_mut(shared1, shared2, m);

#if defined(__cplusplus) && __cplusplus >= 201703L
    auto &[handle1, handle2, lock] = locks;
#else // #if defined(__cplusplus) && __cplusplus >= 201703L
    auto &handle1 = std::get<0>(locks);
    auto &handle2 = std::get<1>(locks);
    auto &lock = std::get<2>(locks);
#endif // #if defined(__cplusplus) && __cplusplus >= 201703L

    REQUIRE(handle1->get_data() == 1);
    REQUIRE(*handle2 == 2.f);
    handle1->do_things_mut();
    *handle2 += 8.f;
    REQUIRE(handle1->get_data() == 3);
    REQUIRE(*handle2 == 10.f);
    REQUIRE(lock.owns_lock());
}

TEST_CASE("Synchronous lock used by mustex", "[mustex]")
{
    Mustex<MyClass> shared1(1);
    Mustex<float> shared2(2.f);
    std::mutex m;

    auto tic = std::chrono::high_resolution_clock::now();

    std::atomic<bool> started{false};
    auto future = std::async(
        [&shared1, &started]
        {
            auto handle = shared1.lock();
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    auto locks = lock_mut(shared1, shared2, m);

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}

TEST_CASE("Synchronous lock used by mutex", "[mustex]")
{
    Mustex<MyClass> shared1(1);
    Mustex<float> shared2(2.f);
    std::mutex m;

    auto tic = std::chrono::high_resolution_clock::now();

    std::atomic<bool> started{false};
    auto future = std::async(
        [&m, &started]
        {
            std::lock_guard<decltype(m)> lock(m);
            started = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    );
    // Make sure the future starts.
    while (!started)
        ;

    auto locks = lock_mut(shared1, shared2, m);

    auto tac = std::chrono::high_resolution_clock::now();
    REQUIRE(tac - tic >= std::chrono::milliseconds(100));
}

TEST_CASE("Synchronous try lock", "[mustex]")
{
    Mustex<MyClass> shared1(1);
    Mustex<float> shared2(2.f);
    std::mutex m;

    {
        // Try lock while nothing is used.
        auto locks = try_lock_mut(shared1, shared2, m);

        REQUIRE(locks);
#if defined(__cplusplus) && __cplusplus >= 201703L
        auto &[handle1, handle2, lock] = *locks;
#else // #if defined(__cplusplus) && __cplusplus >= 201703L
        auto &handle1 = std::get<0>(*locks);
        auto &handle2 = std::get<1>(*locks);
        auto &lock = std::get<2>(*locks);
#endif // #if defined(__cplusplus) && __cplusplus >= 201703L

        REQUIRE(handle1->get_data() == 1);
        REQUIRE(*handle2 == 2.f);
        handle1->do_things_mut();
        *handle2 += 8.f;
        REQUIRE(handle1->get_data() == 3);
        REQUIRE(*handle2 == 10.f);
        REQUIRE(lock.owns_lock());
    }
    {
        // Now try lock while a mustex handle exists.
        std::atomic<bool> started{false};
        auto future = std::async(
            [&shared1, &started]
            {
                auto handle1 = shared1.lock();
                started = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        );
        // Make sure the future starts.
        while (!started)
            ;
        auto locks = try_lock_mut(shared1, shared2, m);
        REQUIRE_FALSE(locks);
        future.wait();
    }
    {
        // Now try lock while a mutex lock_guard exists.
        std::atomic<bool> started{false};
        auto future = std::async(
            [&m, &started]
            {
                std::lock_guard<decltype(m)> lock(m);
                started = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        );
        // Make sure the future starts.
        while (!started)
            ;
        auto locks = try_lock_mut(shared1, shared2, m);
        REQUIRE_FALSE(locks);
        future.wait();
    }
}

TEST_CASE("Mustex with BasicLockable only", "[mustex]")
{
    Mustex<float, BasicLockable> m(2.f);

    auto handle = m.lock();
    REQUIRE(*handle == 2.f);
}

TEST_CASE("Mustex with Lockable only", "[mustex]")
{
    Mustex<float, Lockable> m(2.f);

    {
        auto handle = m.lock();
        REQUIRE(*handle == 2.f);
    }
    {
        auto handle = m.try_lock();
        REQUIRE(handle);
    }
}