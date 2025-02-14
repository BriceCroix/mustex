#ifndef BCX_MUSTEX_HPP
#define BCX_MUSTEX_HPP

#if __cplusplus >= 201103L && __cplusplus < 201703L
#    include <mutex>
#elif __cplusplus >= 201703L
#    include <shared_mutex>
#else
#    error Unsupported C++ standard.
#endif

namespace bcx
{

using MustexMutexType =
#if __cplusplus >= 201703L
    std::shared_mutex
#else
    std::mutex
#endif // #if __cplusplus >= 201703L
    ;

template<typename T>
class Mustex;

#if __cplusplus >= 201703L
template<typename T>
class MustexHandle
{
public:
    friend class Mustex<T>;

    const T &operator*() const
    {
        return m_data;
    }

    const T *operator->() const
    {
        return &m_data;
    }

private:
    std::shared_lock<std::shared_mutex> m_lock;
    const T &m_data;

    MustexHandle(std::shared_mutex &mutex, const T &data)
        : m_lock(mutex)
        , m_data{data} {}
};
#endif // #if __cplusplus >= 201703L

template<typename T>
class MustexHandleMut
{
public:
    friend class Mustex<T>;

    T &operator*()
    {
        return m_data;
    }

    T *operator->()
    {
        return &m_data;
    }

private:
    std::unique_lock<MustexMutexType> m_lock;
    T &m_data;

    MustexHandleMut(MustexMutexType &mutex, T &data)
        : m_lock(mutex)
        , m_data{data}
    {
    }
};

template<typename T>
class Mustex
{
public:
    Mustex(T &&data)
        : m_data{data}
        , m_mutex{}
    {
    }

#if __cplusplus >= 201703L
    MustexHandle<T> lock()
    {
        return MustexHandle<T>(m_mutex, m_data);
    }
#endif // #if __cplusplus >= 201703L

    MustexHandleMut<T> lock_mut()
    {
        return MustexHandleMut<T>(m_mutex, m_data);
    }

private:
    T m_data;
    MustexMutexType m_mutex;
};
} // namespace bcx

#endif // #ifndef BCX_MUSTEX_HPP