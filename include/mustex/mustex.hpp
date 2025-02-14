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

template<typename T, typename L>
class MustexHandle
{
public:
    friend class Mustex<typename std::remove_const<T>::type>;

    MustexHandle() = delete;
    MustexHandle(const MustexHandle &) = delete;
    MustexHandle(MustexHandle &&other)
        : m_lock{std::move(other.m_lock)}, m_data{other.m_data}
    {
    }

    MustexHandle &operator=(const MustexHandle &other) = delete;
    MustexHandle &operator=(MustexHandle &&other)
    {
        m_data = other.m_data;
        m_lock = std::move(other.m_lock);
    }

    virtual ~MustexHandle() = default;

    T &operator*()
    {
        return m_data;
    }

    T *operator->()
    {
        return &m_data;
    }

private:
    L m_lock;
    T &m_data;

    MustexHandle(MustexMutexType &mutex, T &data)
        : m_lock(mutex)
        , m_data{data}
    {
    }
};

template<class T>
class Mustex
{
public:
    template <typename... Args>
    Mustex(Args&&... args)
        : m_data(std::forward<Args>(args)...)
        , m_mutex{}
    {
    }

    Mustex(const Mustex &) = delete; // FIXME enable this if T has a copy constructor.
    Mustex(Mustex &&other) = delete; // FIXME enable this by finding a way to make sure other has no handle around.

    Mustex &operator=(const Mustex &other) = delete; // FIXME enable this if T has a copy constructor.
    Mustex &operator=(Mustex &&other) = delete; // FIXME enable this by finding a way to make sure other has no handle around.

    virtual ~Mustex() = default;

#if __cplusplus >= 201703L
    MustexHandle<const T, std::shared_lock<MustexMutexType>> lock()
    {
        return MustexHandle<const T, std::shared_lock<MustexMutexType>>(m_mutex, m_data);
    }
#endif // #if __cplusplus >= 201703L

    MustexHandle<T, std::unique_lock<MustexMutexType>> lock_mut()
    {
        return MustexHandle<T, std::unique_lock<MustexMutexType>>(m_mutex, m_data);
    }

private:
    T m_data;
    MustexMutexType m_mutex;
};
} // namespace bcx

#endif // #ifndef BCX_MUSTEX_HPP