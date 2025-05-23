# Mustex

![master status](https://github.com/BriceCroix/mustex/actions/workflows/actions.yml/badge.svg?branch=master)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/BriceCroix/mustex/master/LICENSE)

Mustex is a header-only library allowing to make sure a variable protected by a mutex can only be
accessed while the mutex is locked, while leveraging
[RAII](https://en.cppreference.com/w/cpp/language/raii) to abstract synchronization away.

This library is highly inspired by [Rust Mutex](https://doc.rust-lang.org/std/sync/struct.Mutex.html)
and tries to mimic its functionalities.

## Features

- Data-owning mutex type `Mustex<T>` preventing from *forgetting* to lock data.
- Scoped-based synchronization thanks to `MustexHandle<T>`.
- One writer at a time with `lock_mut()`, `try_lock_mut()`, `try_lock_mut_for(...)`, `try_lock_mut_until(...)`

```cpp
bcx::Mustex<float> mustex(42.0f);
{
    auto handle = mustex.lock_mut();
    *handle = 3.14f;
    std::cout << "Pi is " << *handle << std::endl;
}
```

- Multiple simultaneous readers with `lock()`, `try_lock()`, `try_lock_for(...)`, `try_lock_until(...)`.

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

- Deadlock-free multiple access with free standing methods `lock_mut()` and `try_lock_mut()`.

```cpp
bcx::Mustex<int> mustex1(1);
bcx::Mustex<float> mustex2(2.f);
std::mutex m;
{
    auto [handle1, handle2, lock] = bcx::lock_mut(shared1, shared2, m);
    // ...
}
{
    auto res = bcx::try_lock_mut(shared1, shared2, m);
    if (res)
    {
        auto &[handle1, handle2, lock] = *res;
        // ...
    }
}
```

## Integration

[`mustex.hpp`](include/mustex/mustex.hpp) is the only file required to use in your project.
Make sure to have this file available for your compiler and you are good to go !

### Using CMake FetchContent

You may use CMake to automatically download and include this library in your project.
In that case you may add the following lines to your `CMakeLists.txt` file.

```cmake
include(FetchContent)

FetchContent_Declare(
  mustex
  GIT_REPOSITORY https://github.com/BriceCroix/mustex.git
  GIT_TAG        v1.0.0
)
FetchContent_MakeAvailable(mustex)

target_link_libraries(myTarget PRIVATE bcx_mustex)
```

### Using CMake with git submodules

You may add this repository as a git submodule in your project.
In that case you may add the following lines to your `CMakeLists.txt` file.

```cmake
add_subdirectory(path/to/submodule/mustex)

target_link_libraries(myTarget PRIVATE bcx_mustex)
```

## Motivations

Much too often are there code-bases with variables protected by a mutex with only their name to
indicate that they must be locked to do some things. But in a class like this :

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

The only exception to that is when `std::move`ing a handle, the old instance now not owning any
lock on the data anymore, but still being able to be dereferenced.
See [Common pitfalls](#common-pitfalls).

### Why is there no `unlock()`/`lock()` methods on the handle class ?

Having these two method would require either to make it possible for the user to access data in an
unlocked state, or require to always return an `optional` when accessing data, which would be quite
a burden.

### Why is there no free-standing deadlock-free methods `lock()` and `try_lock()` ?

The free standing methods `lock_mut(...)` and `try_lock_mut(...)` rely on the standard methods
[`std::lock()`](https://en.cppreference.com/w/cpp/thread/lock) and
[`std::try_lock()`](https://en.cppreference.com/w/cpp/thread/try_lock), and the standard does not
provide methods `std::lock_shared()`, `std::try_lock_shared` to accomplish that (at least to the
at the moment this file is redacted, with C++23). Although it would be possible to implement these
method ourselves, this would provide small to no benefit compared to the mutable equivalent since
the deadlock-avoidance algorithm is already a quite demanding process.

## Common pitfalls

- Locking twice the same `Mustex` on the same thread in the same scope.
  - In mutable mode (`lock_mut()`) : will cause a deadlock.
  - In read-only mode (`lock()`) : is undefined behavior.

- Keeping a reference to the underlying data that outlives the lifetime of the handle.

```cpp
Mustex<SomeClass> data();
// ... Send reference to other threads
SomeClass *ref;
{
    auto handle = data.lock_mut();
    ref = &(*handle);
    ref->do_things();
}
ref->do_things(); // WARNING This call is undefined behavior (handle is dropped).
```

- Locking two instances mutably and sequentially in same scope and in different orders in two (or more) threads.
The following code can (and eventually will) cause a deadlock :

```cpp
bcx::Mustex<SomeClass> m1;
bcx::Mustex<Whatever> m2;
std::launch(
    [&m1, &m2]
    {
        auto handle1 = m1.lock();
        // Do things with handle1...
        auto handle2 = m2.lock_mut();
        // Do things with handle2... If has not already deadlocked...
    }
);
std::launch(
    [&m1, &m2]
    {
        auto handle2 = m2.lock();
        // Do things with handle2...
        auto handle1 = m1.lock_mut();
        // Do things with handle1... If has not already deadlocked...
    }
);
```

Avoid any risk of deadlock with provided `bcx::lock_mut` deadlock-free method :

```cpp
bcx::Mustex<SomeClass> m1;
bcx::Mustex<Whatever> m2;
std::launch(
    [&m1, &m2]
    {
        auto [handle1, handle2] = bcx::lock_mut(m1, m2);
        // Do things with handle1...
        // Then do things with handle2.
    }
);
std::launch(
    [&m1, &m2]
    {
        auto [handle2, handle1] = bcx::lock_mut(m2, m1);
        // Do things with handle2...
        // Then with handle1.
    }
);
```

Or even better, if it is not necessary to have handles on both data at the same time, scope the
lifetime of the handles in order not to have them living simultaneously.

- Using a handle that has been moved :

```cpp
bcx::Mustex<int> data(42);
auto handle = data.lock();
decltype(handle) handle2(std::move(handle));
int very_dangerous = *handle; // WARNING : NEVER USE A MOVED HANDLE (will crash).
```

## More realistic example

The following example demonstrates how it is possible to have 2 consumers for 1 producer sharing
accesses to a single value. In a real-case scenario the consumers could be a logger-thread and a
thread to notify clients on the network for instance.

The two consumers will be able to read the value at the same time providing the producer is not
currently writing to it.

Notice the use of `std::shared_ptr` to share the ownership of the `Mustex`, mimicking [the common
`Arc<Mutex<T>>` pattern in rust](https://doc.rust-lang.org/std/sync/struct.Mutex.html#examples).

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

### Using custom mutex types

The `Mustex` class internally uses the mutex classes provided by the standard library :

- `std::shared_timed_mutex` if you are compiling for a C++ standard that features it (C++14 and above).
- `std::timed_mutex` if you are compiling without C++14 support.

For a reason or another, you may want to use another mutex type, maybe one of your own,
instead of the default aforementioned one. This can be accomplished by using the full signature of
the `Mustex` class, that allow to select the type of mutex to be used.

The methods `lock()`, `try_lock()`, `try_lock_for(...)`, `try_lock_until(...)` will enable multiple
readers automatically if provided mutex is respectively *BasicSharedLockable* (provides
`lock_shared()` and `unlock_shared()`),
[*SharedLockable*](https://en.cppreference.com/w/cpp/named_req/SharedLockable) and
[*SharedTimedLockable*](https://en.cppreference.com/w/cpp/named_req/SharedTimedLockable). Otherwise
these methods will still be usable with one reader at a time if the mutex is respectively
[*BasicLockable*](https://en.cppreference.com/w/cpp/named_req/BasicLockable),
[*Lockable*](https://en.cppreference.com/w/cpp/named_req/Lockable) and
[*TimedLockable*](https://en.cppreference.com/w/cpp/named_req/TimedLockable).

```cpp
template<typename T>
using MyMustex = bcx::Mustex<T, MyMutex>;

int main(int argc, char* argv[])
{
    MyMustex<int> value(42);
    // You are now free to use this class as usual.
}
```

### Enable simultaneous multiple readers for C++11

The *simultaneous multiple readers* feature of this library is made possible thanks to
`std::shared_timed_mutex` from C++14 (Although `std::shared_mutex` would be sufficient but it was
only introduced in C++17).

If you can provide your own implementation for this type you can enable this feature by using
the full signature of the `Mustex` class, just like in the [previous section](#using-custom-mutex-types).

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

template<typename T>
using MyMustex = bcx::Mustex<T, MySharedMutex>;

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
cmake --preset lin-test-<gcc|clang>-cpp<11|14|17|20>
cmake --build --preset lin-test-<gcc|clang>-cpp<11|14|17|20>
./build_lin/mustex_tests
```

### Windows

```bash
cmake --preset win-test-<msvc|clang>-cpp<11|14|17|20>
cmake --build --preset win-test-<msvc|clang>-cpp<11|14|17|20>
./build_win/RelWithDebInfo/mustex_tests.exe
```
