#ifndef BCX_MUSTEX_HPP
#define BCX_MUSTEX_HPP

// This should be true for C++20 and above
#if defined(__has_include) && __has_include(<version>)
#    include <version>
#    if defined(__cpp_concepts)
#        define _MUSTEX_HAS_CONCEPTS
#    endif
#    if defined(__cpp_lib_optional)
#        define _MUSTEX_HAS_OPTIONAL
#    endif
#    if defined(__cpp_lib_shared_mutex)
#        define _MUSTEX_HAS_SHARED_MUTEX
#    endif
#else // #if defined(__has_include) && __has_include(<version>)
#    if defined(__cplusplus) && __cplusplus >= 202002LL
#        define _MUSTEX_HAS_CONCEPTS
#    endif
#    if defined(__cplusplus) && __cplusplus >= 201703L
#        define _MUSTEX_HAS_OPTIONAL
#    endif
#    if defined(__cplusplus) && __cplusplus >= 201703L
#        define _MUSTEX_HAS_SHARED_MUTEX
#    endif
#endif // #if defined(__has_include) && __has_include(<version>)

#ifdef _MUSTEX_HAS_CONCEPTS
#    include <concepts>
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

#ifdef _MUSTEX_HAS_OPTIONAL
#    include <optional>
#else // #ifdef _MUSTEX_HAS_OPTIONAL
#    include <memory>
#endif // #ifdef _MUSTEX_HAS_OPTIONAL

#ifdef _MUSTEX_HAS_SHARED_MUTEX
#    include <shared_mutex>
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX

#include <mutex>

namespace bcx
{

using MustexMutexType =
#ifdef _MUSTEX_HAS_SHARED_MUTEX
    std::shared_mutex
#else
    std::mutex
#endif
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

    MustexHandle(L &&lock, T &data)
        : m_lock(std::move(lock))
        , m_data{data}
    {
    }
};

template<class T>
class Mustex
{
public:
    template<typename... Args>
#ifdef _MUSTEX_HAS_CONCEPTS
        // Prevent from using this constructor when argument is a mustex (ref or moved)
        requires(!std::is_same_v<Mustex, std::remove_cvref_t<Args>> && ...)
#endif // #ifdef __cpp_concepts
    Mustex(Args &&...args)
        : m_data(std::forward<Args>(args)...)
        , m_mutex{}
    {
    }

#ifdef _MUSTEX_HAS_CONCEPTS
    Mustex(const Mustex &other)
        requires std::is_copy_constructible<T>::value
        : m_data(*other.lock())
        , m_mutex{}
    {
    }

    Mustex(Mustex &&other)
        requires std::is_move_constructible<T>::value
        : m_data(*other.lock())
        , m_mutex{}
    {
    }
    Mustex &operator=(const Mustex &other)
        requires std::is_assignable<T, const T &>::value
    {
        std::unique_lock lock(m_mutex);
        m_data = *other.lock();
    }
    Mustex &operator=(Mustex &&other)
        requires std::is_assignable<T, T &&>::value
    {
        std::unique_lock lock(m_mutex);
        m_data = std::move(*other.lock_mut());
    }
#else  // #ifdef _MUSTEX_HAS_CONCEPTS
    // Without c++20 these cannot be simply conditionally defined.
    // See this great article as of why : https://akrzemi1.wordpress.com/2015/03/02/a-conditional-copy-constructor/
    // The workaround is to construct Mustex after locking the other :
    //
    // Mustex<int> m(42);
    // Mustex<int> m2(*m.lock()) // Or lock_mut()

    Mustex(const Mustex &) = delete;
    Mustex(Mustex &&other) = delete;
    Mustex &operator=(const Mustex &other) = delete;
    Mustex &operator=(Mustex &&other) = delete;
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

    virtual ~Mustex() = default;

#ifdef _MUSTEX_HAS_SHARED_MUTEX
    MustexHandle<const T, std::shared_lock<MustexMutexType>> lock() const
    {
        return MustexHandle<const T, std::shared_lock<MustexMutexType>>(std::shared_lock(m_mutex), m_data);
    }

    std::optional<MustexHandle<const T, std::shared_lock<MustexMutexType>>> try_lock() const
    {
        std::shared_lock lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return MustexHandle<const T, std::shared_lock<MustexMutexType>>(std::move(lock), m_data);
        return {};
    }
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX

#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<MustexHandle<T, std::unique_lock<MustexMutexType>>> try_lock_mut()
    {
        std::unique_lock lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return MustexHandle<T, std::unique_lock<MustexMutexType>>(std::move(lock), m_data);
        return {};
    }
#else  // #ifdef _MUSTEX_HAS_OPTIONAL
    std::unique_ptr<MustexHandle<T, std::unique_lock<MustexMutexType>>> try_lock_mut()
    {
        std::unique_lock<decltype(m_mutex)> lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return std::unique_ptr<MustexHandle<T, std::unique_lock<MustexMutexType>>>(
                new MustexHandle<T, std::unique_lock<MustexMutexType>>(std::move(lock), m_data)
            );
        return {};
    }
#endif // #ifdef _MUSTEX_HAS_OPTIONAL

    MustexHandle<T, std::unique_lock<MustexMutexType>> lock_mut()
    {
        return MustexHandle<T, std::unique_lock<MustexMutexType>>(std::unique_lock(m_mutex), m_data);
    }

private:
    T m_data;
    mutable MustexMutexType m_mutex;
};
} // namespace bcx

#endif // #ifndef BCX_MUSTEX_HPP