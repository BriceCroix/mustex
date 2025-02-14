# Mustex

Mustex is a header-only library allowing to make sure a variable protected by a mutex can only be
accessed while the mutex is locked. Additionally if C++17 standard is used, the variable can be read
simultaneously by several thread, as long as it is not locked mutably.

This library is highly inspired by [Rust Mutex](https://doc.rust-lang.org/std/sync/struct.Mutex.html)
and tries to mimic its functionalities.

## Example

```cpp
Mustex<int> mustex(42);

{
    // Lock in readonly mode.
    auto handle = mustex.lock();
    std::cout << "My int is " << *handle << std::endl;
    // Can be locked in readonly mode twice !
    auto handle2 = mustex.lock();
    std::cout << "My int is still" << *handle2 << std::endl;
}
{
    // Lock to write.
    auto handle = mustex.lock_mut();
    std::cout << "My int is " << *handle << std::endl;
    *handle = 12;
    std::cout << "And is now " << *handle << std::endl;
}
```

## Building tests

### Unix

```bash
cmake --preset lin . && cmake --build build_lin -j$(nproc)
./build_lin/mustex_tests
```

### Windows

```bash
cmake.exe --preset win-vs16-2019 . && cmake --build build_win --config RelWithDebInfo
./build_win/RelWithDebInfo/mustex_tests.exe
```
