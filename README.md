# Mustex

![master status](https://github.com/BriceCroix/mustex/actions/workflows/actions.yml/badge.svg?branch=master)

Mustex is a header-only library allowing to make sure a variable protected by a mutex can only be
accessed while the mutex is locked, while leveraging
[RAII](https://en.cppreference.com/w/cpp/language/raii) to abstract synchronization away.

This library is highly inspired by [Rust Mutex](https://doc.rust-lang.org/std/sync/struct.Mutex.html)
and tries to mimic its functionalities.

## Features

- Data-owning mutex type `Mustex<T>` preventing from *forgetting* to lock data.
- Scoped-based synchronization thanks to `MustexHandle<T>`.
- One writer at a time with `lock_mut()`, `try_lock_mut()`

```cpp
bcx::Mustex<float> mustex(42.0f);
{
    auto handle = mustex.lock_mut();
    *handle = 3.14f;
    std::cout << "Pi is " << *handle << std::endl;
}
```

- Multiple simultaneous readers with `lock()`, `try_lock()`.

```cpp
bcx::Mustex<std::string> name("Batman");
auto handle = name.lock();
auto future = std::async(
    [&name]
    {
        auto handle = name.lock();
        std::cout << "Hello " << *handle << " from child thread !" << std::endl;
    }
);
std::cout << "Hello " << *handle << " from parent thread !" << std::endl;
future.wait();
// Yay that does not deadlock !
```

> [!NOTE]  
> Simultaneous readers are enabled by default using C++14 and above, but can be enabled for C++11
> by providing your own implementation of *SharedLockable* mutexes (or from third-party).
> See [relevant section](#enable-simultaneous-multiple-readers-for-c11) on how to do this.

- Deadlock-free multiple access.

```cpp
bcx::Mustex<int> mustex1(1);
bcx::Mustex<float> mustex2(2.f);
std::mutex m;
{
    auto [handle1, handle2, lock] = bcx::lock_mut(shared1, shared2, m);
    // ...
}
```

## Motivations

Much too often are there code-bases with variable(s) protected by a mutex with only its name to
indicate that it must be locked to do *some* things. But in a class like this :

```cpp
class VeryBigClass
{
    /* some members and methods here */

    std::string m_name;
    std::mutex m_mutex;
    std::string m_code;
}
```

What is protecting `m_mutex` ? `m_name` ? `m_code` ? Both ?
As a developer that has to add a method to this class and want to read one or both of these
variables, what prevents you to access them without locking `m_mutex`, effectively creating a race
condition ? `Mustex` addresses this issue by making it *impossible* to access the data without
locking its protecting mutex.

## Design choices

The main reason for every design choice of this library is the following : *If you have a handle on
a Mustex, you are free to do anything with the data*. The thread that called `lock()` may be blocked
for a certain time, but when a handle is returned, access is granted until the destruction of the
handle.

So at the question *Why is there no `unlock()`/`lock()` ?* on the handle class, the answer is that
having these two method would require either to make it possible for the user to access data in an
unlocked state, or to always return an `optional` when accessing data, which would be quite a burden.

The only exception to that is when `std::move`ing instances, the old instance now not owning any
lock on the data anymore, but still being able to access it :

```cpp
bcx::Mustex<int> data(42);
auto handle = data.lock();
decltype(handle) handle2(std::move(handle));
int very_dangerous = *handle; // WARNING : NEVER USE A MOVED HANDLE.
```

## Common pitfalls

- Locking twice the same `Mustex` on the same thread in the same scope.
  - In mutable mode (`lock_mut()`) : will cause a deadlock.
  - In read-only mode (`lock()`) : is undefined behavior.

## More realistic example

The following example demonstrates how it is possible to have 2 consumers for 1 producer sharing
accesses to a single value. In a real-case scenario the consumers could be a logger-thread and a
thread to notify clients on the network for instance.

The two consumers will be able to read the value at the same time providing the producer is not
currently writing to it.

Notice the use of `shared_ptr` to share the ownership of the `Mustex`, mimicking the common
`Arc<Mutex<T>>` pattern in rust.

```cpp
#include <atomic>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mustex/mustex.hpp>
#include <thread>

/// Mimics a long-lasting computation.
int compute_next(int current)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    return 2 * current - 1;
}

int main(int argc, char *argv[])
{
    auto value = std::make_shared<bcx::Mustex<int>>(42);

    std::atomic<bool> running(true);

    auto producer = std::async(
        [&running, value]
        {
            while (running)
            {
                int current = 0;
                {
                    auto handle = value->lock();
                    current = *handle;
                }
                int next = compute_next(current);
                {
                    auto handle = value->lock_mut();
                    *handle = next;
                }
            }
        }
    );

    auto consumer1 = std::async(
        [&running, value]
        {
            while (running)
            {
                int current = 0;
                {
                    auto handle = value->lock();
                    current = *handle;
                }
                std::cout << "Current value is " << current << std::endl;

                // Log each second.
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        }
    );

    auto consumer2 = std::async(
        [&running, value]
        {
            std::ofstream file("myfile.txt");
            while (running)
            {
                int current = 0;
                {
                    auto handle = value->lock();
                    current = *handle;
                }
                file << current << std::endl;

                // Write to file at regular intervals.
                std::this_thread::sleep_for(std::chrono::milliseconds(17));
            }
        }
    );

    // Run for one minute.
    std::this_thread::sleep_for(std::chrono::seconds(60));
    running = false;
    producer.wait();
    consumer1.wait();
    consumer2.wait();

    std::cout << "Goodbye !" << std::endl;
}
```

## Advanced use

### Using custom mutex or lock types

The `Mustex` class internally uses the mutexes and locks classes provided by the standard library :
- `std::shared_timed_mutex` if you are compiling for a c++ standard that features it.
- `std::mutex` if you are compiling without C++14 support.
- `std::unique_lock` for write-accesses to the protected data.
  Also used for read-accesses if compiling for a c++ standard that does not feature `std::shared_lock`.
- `std::shared_lock` for read-accesses to the protected data.

For a reason or another, you may want to use your own mutex and lock types instead of the ones
provided by the `stdlib`. This can be accomplished by using the full signature of the `Mustex`
class, that allow to select the mentioned classes.

```cpp
template<typename T>
using MyMustex = bcx::Mustex<T, MySharedMutex, MyReadLock, MyWriteLock>;

int main(int argc, char* argv[])
{
    MyMustex<int> value(42);
    // You are now free to use this class as usual.
}
```

### Enable simultaneous multiple readers for C++11

The *simultaneous multiple readers* feature of this library is made possible thanks to
`std::shared_timed_mutex` and `std::shared_lock` from C++14.

If you can provide your own implementation for these two types you can enable this feature by using
the full signature of the `Mustex` class, just like in the [previous section](#using-custom-mutex-or-lock-types).

You may also use third-party implementations such as
[Boost's](http://www.boost.org/doc/libs/1_41_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_types.shared_mutex),
[POSIX's](https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032u/index.html),
[Win32's](http://msdn.microsoft.com/en-us/library/windows/desktop/aa904937%28v=vs.85%29.aspx),
[HowardHinnant's](https://howardhinnant.github.io/shared_mutex.cpp),
[Emanem's](https://github.com/Emanem/shared_mutex), etc.

```cpp
#include <mustex/mustex.hpp>

class MySharedMutex
{
    // ...
}

template <typename M>
class MySharedLock
{
    // ...
}

template<typename T>
using MyMustex = bcx::Mustex<T, MySharedMutex, MySharedLock, std::unique_lock>;

int main(int argc, char* argv[])
{
    MyMustex<int> value(42);
    auto handle = value.lock();
    auto future = std::async([&value]{ auto handle = value.lock(); });
    future.wait();
    // Yay that does not deadlock !
}

```

## Supported OS and compilers

|       |       Linux        |         Windows         |
| ----- | :----------------: | :---------------------: |
| gcc   | :heavy_check_mark: | (i.e. mingw) Not tested |
| msvc  |         NA         |   :heavy_check_mark:    |
| clang | :heavy_check_mark: |   :heavy_check_mark:    |

## C++ standards differences

|                      |                            C++11                            |       C++14        |       C++17        |       C++20        |
| -------------------- | :---------------------------------------------------------: | :----------------: | :----------------: | :----------------: |
| Thread safety        |                     :heavy_check_mark:                      | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Simultaneous readers | :x: [But...](#enable-simultaneous-multiple-readers-for-c11) | :heavy_check_mark: | :heavy_check_mark: | :heavy_check_mark: |
| Optional return type |                      `std::unique_ptr`                      | `std::unique_ptr`  |  `std::optional`   |  `std::optional`   |
| Copy/Move Mustex     |                             :x:                             |        :x:         |        :x:         | :heavy_check_mark: |

## Building tests

### Unix

```bash
cmake --preset lin-test-<gcc|clang>-cpp<11|17|20>
cmake --build --preset lin-test-<gcc|clang>-cpp<11|17|20>
./build_lin/mustex_tests
```

### Windows

```bash
cmake --preset win-test-<msvc|clang>-cpp<11|17|20>
cmake --build --preset win-test-<msvc|clang>-cpp<11|17|20>
./build_win/RelWithDebInfo/mustex_tests.exe
```
