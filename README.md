# Mustex

![master status](https://github.com/BriceCroix/mustex/actions/workflows/actions.yml/badge.svg?branch=master)

Mustex is a header-only library allowing to make sure a variable protected by a mutex can only be
accessed while the mutex is locked, while leveraging [RAII](https://en.cppreference.com/w/cpp/language/raii)
to abstract synchronization away.

This library is highly inspired by [Rust Mutex](https://doc.rust-lang.org/std/sync/struct.Mutex.html)
and tries to mimic its functionalities.

## Features

- One writer at a time.

```cpp
bcx::Mustex<float> mustex(42.0f);
{
    auto handle = mustex.lock_mut();
    *handle = 3.14f;
    std::cout << "Pi is " << *handle << std::endl;
}
```

- Multiple simultaneous readers.

```cpp
bcx::Mustex<int> mustex(42);
{
    auto handle = mustex.lock();
    auto handle2 = mutex.lock();
    // No deadlock here !
    std::cout << *handle << "==" << handle2 << std::endl;
}
```

> [!NOTE]  
> Simultaneous readers are enabled by default using C++17 and above, but can be enabled for C++11
> by providing your own implementation of *SharedLockable* mutexes (or from third-party).
> See [relevant section](#enable-simultaneous-multiple-readers-for-c11) on how to do this.

## More realistic example

The following example demonstrates how it is possible to have 2 consumers for 1 producer sharing accesses to a single value. In a real-case scenario the consumers could be a logger-thread and a thread to notify clients on the network for instance.

The two consumers will be able to read the value at the same time providing the producer is not currently writing to it.

Notice the use of `shared_ptr` to share the ownership of the `Mustex`, mimicking the common `Arc<Mutex<T>>` pattern in rust.

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
        std::launch::async,
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
        });

    auto consumer1 = std::async(
        std::launch::async,
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
        });

    auto consumer2 = std::async(
        std::launch::async,
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
        });

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

### Using custom mutex type

TODO

### Enable simultaneous multiple readers for C++11

The *simultaneous multiple readers* feature of this library is made possible thanks to `std::shared_mutex` and `std::shared_lock` from C++17.

If you can provide your own implementation for these two types you can enable this feature by using the full signature of the `Mustex` class.

You may also use third-party implementations such as [Boost's](http://www.boost.org/doc/libs/1_41_0/doc/html/thread/synchronization.html#thread.synchronization.mutex_types.shared_mutex), [POSIX's](https://docs.oracle.com/cd/E19455-01/806-5257/6je9h032u/index.html), [Win32's](http://msdn.microsoft.com/en-us/library/windows/desktop/aa904937%28v=vs.85%29.aspx), [HowardHinnant's](https://howardhinnant.github.io/shared_mutex.cpp), [Emanem's](https://github.com/Emanem/shared_mutex), etc.

Alternatively if you are using C++14, `std::shared_lock` entered the standard in this version, but not `std::shared_mutex`...

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
using MyMustex = bcx::Mustex<T, MySharedMutex, MySharedLock<MySharedMutex>, std::unique_lock>;

int main(int argc, char* argv[])
{
    MyMustex<int> value(42);
    auto handle = value.lock();
    auto handle2 = value.lock();
    // Yay that does not deadlock !
}

```

## Supported OS and compilers

|       |        Linux       |         Windows         |
|-------|:------------------:|:-----------------------:|
| gcc   | :heavy_check_mark: | (i.e. mingw) Not tested |
| msvc  |         NA         |    :heavy_check_mark:   |
| clang | :heavy_check_mark: |    :heavy_check_mark:   |

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
