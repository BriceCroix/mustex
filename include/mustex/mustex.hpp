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
#    if defined(__cpp_lib_shared_timed_mutex)
#        define _MUSTEX_HAS_SHARED_MUTEX
#    endif
#    if defined(__cpp_lib_integer_sequence)
#        define _MUSTEX_HAS_INT_SEQUENCE
#    endif
#else // #if defined(__has_include) && __has_include(<version>)
#    if defined(__cplusplus) && __cplusplus >= 202002LL
#        define _MUSTEX_HAS_CONCEPTS
#    endif
#    if defined(__cplusplus) && __cplusplus >= 201703L
#        define _MUSTEX_HAS_OPTIONAL
#    endif
#    if defined(__cplusplus) && __cplusplus >= 201402L
#        define _MUSTEX_HAS_SHARED_MUTEX
#        define _MUSTEX_HAS_INT_SEQUENCE
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

#include <chrono>
#include <mutex>
#include <utility>

namespace bcx
{

// Forward declares
template<typename T, class M>
class Mustex;

namespace detail
{
#ifdef _MUSTEX_HAS_SHARED_MUTEX
using DefaultMustexMutex = std::shared_timed_mutex;
#else // #ifdef _MUSTEX_HAS_SHARED_MUTEX
using DefaultMustexMutex = std::timed_mutex;
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX

/// @brief Concept class whose member `value` indicates if a mutex is BasicLockable.
/// https://en.cppreference.com/w/cpp/named_req/BasicLockable
/// @tparam T Type of mutex to check.
template<typename T>
class is_basic_lockable
{
private:
    template<typename U>
    static auto test(int) -> decltype(std::declval<U>().lock(), std::declval<U>().unlock(), std::true_type{});

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

/// @brief Concept class whose member `value` indicates if a mutex is Lockable.
/// https://en.cppreference.com/w/cpp/named_req/Lockable
/// @tparam T Type of mutex to check.
template<typename T>
class is_lockable
{
private:
    template<typename U>
    static auto test(int) -> decltype(std::is_same<decltype(std::declval<U>().try_lock()), bool>{}, // must return bool
                                      std::true_type{});

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value && is_basic_lockable<T>::value;
};

/// @brief Concept class whose member `value` indicates if a mutex is TimedLockable.
/// https://en.cppreference.com/w/cpp/named_req/TimedLockable
/// @tparam T Type of mutex to check.
template<typename T>
class is_timed_lockable
{
private:
    template<typename U, typename Rep, typename Period, typename Clock, typename Duration>
    static auto test(int) -> decltype(std::is_same<decltype(std::declval<U>().try_lock_for(std::declval<const std::chrono::duration<Rep, Period> &>())), bool>{}, // Must return bool
                                      std::is_same<decltype(std::declval<U>().try_lock_until(std::declval<const std::chrono::time_point<Clock, Duration> &>())), bool>{}, // Must return bool
                                      std::true_type{});

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value && is_lockable<T>::value;
};

/// @brief Concept class whose member `value` indicates if a mutex is BasicSharedLockable.
/// This named requirement is not standard but easily derived from BasicLockable/Lockable.
/// @tparam T Type of mutex to check.
template<typename T>
class is_basic_shared_lockable
{
private:
    template<typename U>
    static auto test(int) -> decltype(std::declval<U>().lock_shared(), // must be valid
                                      std::declval<U>().unlock_shared(), // must be valid
                                      std::true_type{});

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value;
};

/// @brief Concept class whose member `value` indicates if a mutex is SharedLockable.
/// https://en.cppreference.com/w/cpp/named_req/SharedLockable
/// @tparam T Type of mutex to check.
template<typename T>
class is_shared_lockable
{
private:
    template<typename U>
    static auto test(int) -> decltype(std::is_same<decltype(std::declval<U>().try_lock_shared()), bool>{}, // must return bool
                                      std::true_type{});

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value && is_basic_shared_lockable<T>::value;
};

/// @brief Concept class whose member `value` indicates if a mutex is SharedTimedLockable.
/// https://en.cppreference.com/w/cpp/named_req/SharedTimedLockable
/// @tparam T Type of mutex to check.
template<typename T>
class is_shared_timed_lockable
{
private:
    template<typename U, typename Rep, typename Period, typename Clock, typename Duration>
    static auto test(int) -> decltype(std::is_same<decltype(std::declval<U>().try_lock_shared_for(std::declval<const std::chrono::duration<Rep, Period> &>())), bool>{}, // Must return bool
                                      std::is_same<decltype(std::declval<U>().try_lock_shared_until(std::declval<const std::chrono::time_point<Clock, Duration> &>())), bool>{}, // Must return bool
                                      std::true_type{});

    template<typename>
    static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<T>(0))::value && is_shared_lockable<T>::value;
};

/// @brief Methods to redirect read/write lock accesses to mutex,
/// depending on whether or not the mutex is shared lockable.
namespace proxy_mutex
{
template<typename M>
inline typename std::enable_if<is_basic_shared_lockable<M>::value, void>::type
    lock_read(M &m)
{
    m.lock_shared();
}
template<typename M>
inline typename std::enable_if<!is_basic_shared_lockable<M>::value, void>::type
    lock_read(M &m)
{
    m.lock();
}
template<typename M>
inline void lock_write(M &m)
{
    m.lock();
}

template<typename M>
inline typename std::enable_if<is_shared_lockable<M>::value, bool>::type
    try_lock_read(M &m)
{
    return m.try_lock_shared();
}
template<typename M>
inline typename std::enable_if<!is_shared_lockable<M>::value, bool>::type
    try_lock_read(M &m)
{
    return m.try_lock();
}

template<typename M, typename Rep, typename Period>
inline typename std::enable_if<is_shared_timed_lockable<M>::value, bool>::type
    try_lock_read_for(M &m, const std::chrono::duration<Rep, Period> &d)
{
    return m.try_lock_shared_for(d);
}
template<typename M, typename Rep, typename Period>
inline typename std::enable_if<!is_shared_timed_lockable<M>::value, bool>::type
    try_lock_read_for(M &m, const std::chrono::duration<Rep, Period> &d)
{
    return m.try_lock_for(d);
}

template<typename M, typename Clock, typename Duration>
inline typename std::enable_if<is_shared_timed_lockable<M>::value, bool>::type
    try_lock_read_until(M &m, const std::chrono::time_point<Clock, Duration> &tp)
{
    return m.try_lock_shared_until(tp);
}
template<typename M, typename Clock, typename Duration>
inline typename std::enable_if<!is_shared_timed_lockable<M>::value, bool>::type
    try_lock_read_until(M &m, const std::chrono::time_point<Clock, Duration> &tp)
{
    return m.try_lock_until(tp);
}

template<typename M, typename Rep, typename Period>
inline bool try_lock_write_for(M &m, const std::chrono::duration<Rep, Period> &d)
{
    return m.try_lock_for(d);
}

template<typename M, typename Clock, typename Duration>
inline bool try_lock_write_until(M &m, const std::chrono::time_point<Clock, Duration> &tp)
{
    return m.try_lock_until(tp);
}

template<typename M>
inline bool try_lock_write(M &m)
{
    return m.try_lock();
}

template<typename M>
inline typename std::enable_if<is_basic_shared_lockable<M>::value, void>::type
    unlock_read(M &m)
{
    m.unlock_shared();
}
template<typename M>
inline typename std::enable_if<!is_basic_shared_lockable<M>::value, void>::type
    unlock_read(M &m)
{
    m.unlock();
}
template<typename M>
inline void unlock_write(M &m)
{
    m.unlock();
}
} // namespace proxy_mutex

template<typename U>
struct is_mustex : std::false_type
{
};

template<typename T, class M>
struct is_mustex<Mustex<T, M>> : std::true_type
{
};

/// @brief Extract mutex reference from raw mutex.
template<typename U>
auto get_mutex_ref(U &m) -> typename std::enable_if<!is_mustex<U>::value, U &>::type
{
    return m;
}

/// @brief Extract mutex reference from Mustex.
template<typename U>
auto get_mutex_ref(U &m) -> typename std::enable_if<is_mustex<U>::value, typename U::mutex_t &>::type
{
    return m.m_mutex;
}

/// @brief Acquire lock (adopt) for a raw mutex.
template<template<class> class L, typename T>
auto adopt_lock(T &m) -> typename std::enable_if<!is_mustex<T>::value, L<T>>::type
{
    return L<T>(m, std::adopt_lock);
}

/// @brief Acquire lock (adopt) for Mustex.
template<template<class> class L, typename T>
auto adopt_lock(T &m) -> typename std::enable_if<is_mustex<T>::value, typename T::HandleMut>::type
{
    return m.lock_mut(std::adopt_lock);
}

// C++11 does not provide std::index_sequence, implement it ourselves if necessary.
#ifdef _MUSTEX_HAS_INT_SEQUENCE
template<size_t... _Idx>
using bcx_index_sequence = std::index_sequence<_Idx...>;
template<size_t _Num>
using bcx_make_index_sequence = std::make_index_sequence<_Num>;
#else
template<std::size_t... Indices>
struct bcx_index_sequence
{
};

template<std::size_t N, std::size_t... Indices>
struct bcx_make_index_sequence_impl : bcx_make_index_sequence_impl<N - 1, N - 1, Indices...>
{
};

template<std::size_t... Indices>
struct bcx_make_index_sequence_impl<0, Indices...>
{
    typedef bcx_index_sequence<Indices...> type;
};

template<std::size_t N>
using bcx_make_index_sequence = typename bcx_make_index_sequence_impl<N>::type;
#endif

template<typename Tuple, std::size_t... I>
void lock_all_impl(Tuple &mutexes, bcx_index_sequence<I...>)
{
    std::lock(std::get<I>(mutexes)...);
}

template<typename Tuple>
void lock_all(Tuple &mutexes)
{
    constexpr std::size_t N = std::tuple_size<Tuple>::value;
    lock_all_impl(mutexes, bcx_make_index_sequence<N>{});
}

template<typename Tuple, std::size_t... I>
bool try_lock_all_impl(Tuple &mutexes, bcx_index_sequence<I...>)
{
    // Yes -1 is the success code. https://en.cppreference.com/w/cpp/thread/try_lock
    return std::try_lock(std::get<I>(mutexes)...) == -1;
}

/// @brief Try to lock all given mutexes.
/// @return True for success.
template<typename Tuple>
bool try_lock_all(Tuple &mutexes)
{
    constexpr std::size_t N = std::tuple_size<Tuple>::value;
    return try_lock_all_impl(mutexes, bcx_make_index_sequence<N>{});
}

template<template<class> class L, typename... Args>
auto lock_mut_impl(Args &...args) -> std::tuple<decltype(detail::adopt_lock<L>(args))...>
{
    auto mutex_refs = std::tie(detail::get_mutex_ref(args)...);
    detail::lock_all(mutex_refs);
    return std::make_tuple(detail::adopt_lock<L>(args)...);
}

template<template<class> class L, typename... Args>
auto try_lock_mut_impl(Args &...args)
#ifdef _MUSTEX_HAS_OPTIONAL
    -> std::optional<std::tuple<decltype(detail::adopt_lock<L>(args))...>>
#else
    -> std::unique_ptr<std::tuple<decltype(detail::adopt_lock<L>(args))...>>
#endif
{
    auto mutex_refs = std::tie(detail::get_mutex_ref(args)...);
    if (!detail::try_lock_all(mutex_refs))
        return {};
    auto tuple = std::make_tuple(detail::adopt_lock<L>(args)...);
#ifdef _MUSTEX_HAS_OPTIONAL
    return std::move(tuple);
#else
    return std::unique_ptr<std::tuple<decltype(detail::adopt_lock<L>(args))...>>(
        new std::tuple<decltype(detail::adopt_lock<L>(args))...>(std::move(tuple))
    );
#endif
}

} // namespace detail

/// @brief Lock mutably any given Mustex or raw mutex using deadlock avoidance.
/// @tparam L Type of lock to use on raw mutexes.
/// @tparam ...Args Types of given arguments.
/// @param ...args Arbitrary number of Mustex and/or raw mutex.
/// @return Tuple containing a Mustex::HandleMut for each given Mustex, and a lock for each raw mutex.
template<template<class> class L = std::unique_lock, typename... Args>
inline auto lock_mut(Args &...args) -> decltype(detail::lock_mut_impl<L>(args...))
{
    return detail::lock_mut_impl<L>(args...);
}

/// @brief Tries to lock mutably any given Mustex or raw mutex using deadlock avoidance.
/// @tparam L Type of lock to use on raw mutexes.
/// @tparam ...Args Types of given arguments.
/// @param ...args Arbitrary number of Mustex and/or raw mutex.
/// @return Optional tuple. Contains nothing if at least one argument could not be locked.
///         Otherwise contains a Mustex::HandleMut for each given Mustex, and a lock for each raw mutex.
template<template<class> class L = std::unique_lock, typename... Args>
inline auto try_lock_mut(Args &...args) -> decltype(detail::try_lock_mut_impl<L>(args...))
{
    return detail::try_lock_mut_impl<L>(args...);
}

/// @brief Allow to access Mustex data, mutably or not depending on method used to construct.
/// @tparam T Type of data to be accessed, potentially const-qualified.
/// @tparam M Type of mutex owned by this class.
template<typename T, class M>
class MustexHandle
{
private:
    void unlock()
    {
        if (!m_mutex)
            return;
        if (std::is_const<T>::value)
            detail::proxy_mutex::unlock_read(*m_mutex);
        else
            detail::proxy_mutex::unlock_write(*m_mutex);
    }

public:
    // Only parent Mustex can instantiate this class.
    template<class MT, class MM>
    friend class Mustex;

    /// @brief The type of contained value, exposed for convenience.
    using data_t = typename std::remove_cv<T>::type;

    MustexHandle() = delete;
    MustexHandle(const MustexHandle &) = delete;
    MustexHandle(MustexHandle &&other)
        : m_mutex{other.m_mutex}
        , m_data{other.m_data}
    {
        other.m_mutex = nullptr;
        other.m_data = nullptr;
    }

    MustexHandle &operator=(const MustexHandle &other) = delete;
    MustexHandle &operator=(MustexHandle &&other)
    {
        unlock();
        m_data = other.m_data;
        other.m_data = nullptr;
        m_mutex = other.m_mutex;
        other.m_mutex = nullptr;
        return *this;
    }

    virtual ~MustexHandle()
    {
        unlock();
    }

    T &operator*()
    {
        return *m_data;
    }

    T *operator->()
    {
        return m_data;
    }

private:
    M *m_mutex;
    T *m_data;

    /// @brief Create handle on ALREADY ACQUIRED mutex.
    /// @param mutex
    /// @param data
    MustexHandle(M *mutex, T *data)
        : m_mutex{mutex}
        , m_data{data}
    {
    }
};

/// @brief Data-owning mutex class, allowing never to access shared data
/// without thread synchronization.
/// @tparam T The type of data to be shared among threads.
/// @tparam M Type of synchronization mutex.
template<class T, class M = detail::DefaultMustexMutex>
class Mustex
{
public:
    /// @brief The type of contained value, exposed for convenience.
    using data_t = typename std::remove_cv<T>::type;
    /// @brief The type of mutex used, exposed for convenience.
    using mutex_t = M;
    /// @brief The type of handle used to access data.
    using Handle = MustexHandle<const data_t, M>;
    /// @brief The type of handle used to access data mutably.
    using HandleMut = MustexHandle<data_t, M>;

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
        : m_data(std::move(*other.lock_mut()))
        , m_mutex{}
    {
    }
    Mustex &operator=(const Mustex &other)
        requires std::is_assignable<T &, const T &>::value
    {
        std::lock_guard<M> lock(m_mutex);
        m_data = *other.lock();
        return *this;
    }
    Mustex &operator=(Mustex &&other)
        requires std::is_assignable<T &, T &&>::value
    {
        auto lock_and_handle = bcx::lock_mut(m_mutex, other);
        m_data = std::move(*std::get<1>(lock_and_handle));
        return *this;
    }
#else // #ifdef _MUSTEX_HAS_CONCEPTS
    // Without c++20 these cannot be simply conditionally defined.
    // See this great article as of why : https://akrzemi1.wordpress.com/2015/03/02/a-conditional-copy-constructor/
    // The workaround is to directly access data by locking the other :
    //
    // Mustex<int> m(42);
    // Mustex<int> m2(*m.lock());
    // m = *m2.lock();

    Mustex(const Mustex &) = delete;
    Mustex(Mustex &&other) = delete;
    Mustex &operator=(const Mustex &other) = delete;
    Mustex &operator=(Mustex &&other) = delete;
#endif // #ifdef _MUSTEX_HAS_CONCEPTS

    virtual ~Mustex() = default;

private:
#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<Handle>
#else
    std::unique_ptr<Handle>
#endif
        try_lock_impl() const
    {
        if (detail::proxy_mutex::try_lock_read(m_mutex))
#ifdef _MUSTEX_HAS_OPTIONAL
            return Handle(&m_mutex, &m_data);
#else
            return std::unique_ptr<Handle>(new Handle(&m_mutex, &m_data));
#endif
        return {};
    }

    template<typename Rep, typename Period>
#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<Handle>
#else
    std::unique_ptr<Handle>
#endif
        try_lock_for_impl(const std::chrono::duration<Rep, Period> &d) const
    {
        if (detail::proxy_mutex::try_lock_read_for(m_mutex, d))
#ifdef _MUSTEX_HAS_OPTIONAL
            return Handle(&m_mutex, &m_data);
#else
            return std::unique_ptr<Handle>(new Handle(&m_mutex, &m_data));
#endif
        return {};
    }

    template<typename Clock, typename Duration>
#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<Handle>
#else
    std::unique_ptr<Handle>
#endif
        try_lock_until_impl(const std::chrono::time_point<Clock, Duration> &tp) const
    {
        if (detail::proxy_mutex::try_lock_read_until(m_mutex, tp))
#ifdef _MUSTEX_HAS_OPTIONAL
            return Handle(&m_mutex, &m_data);
#else
            return std::unique_ptr<Handle>(new Handle(&m_mutex, &m_data));
#endif
        return {};
    }

#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<HandleMut>
#else
    std::unique_ptr<HandleMut>
#endif
        try_lock_mut_impl()
    {
        if (detail::proxy_mutex::try_lock_write(m_mutex))
#ifdef _MUSTEX_HAS_OPTIONAL
            return HandleMut(&m_mutex, &m_data);
#else
            return std::unique_ptr<HandleMut>(new HandleMut(&m_mutex, &m_data));
#endif
        return {};
    }

    template<typename Rep, typename Period>
#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<HandleMut>
#else
    std::unique_ptr<HandleMut>
#endif
        try_lock_mut_for_impl(const std::chrono::duration<Rep, Period> &d)
    {
        if (detail::proxy_mutex::try_lock_write_for(m_mutex, d))
#ifdef _MUSTEX_HAS_OPTIONAL
            return HandleMut(&m_mutex, &m_data);
#else
            return std::unique_ptr<HandleMut>(new HandleMut(&m_mutex, &m_data));
#endif
        return {};
    }

    template<typename Clock, typename Duration>
#ifdef _MUSTEX_HAS_OPTIONAL
    std::optional<HandleMut>
#else
    std::unique_ptr<HandleMut>
#endif
        try_lock_mut_until_impl(const std::chrono::time_point<Clock, Duration> &tp)
    {
        if (detail::proxy_mutex::try_lock_write_until(m_mutex, tp))
#ifdef _MUSTEX_HAS_OPTIONAL
            return HandleMut(&m_mutex, &m_data);
#else
            return std::unique_ptr<HandleMut>(new HandleMut(&m_mutex, &m_data));
#endif
        return {};
    }

public:
    /// @brief Lock data for read-only access.
    /// @return Handle on owned data.
    Handle lock() const
    {
        detail::proxy_mutex::lock_read(m_mutex);
        return Handle(&m_mutex, &m_data);
    }

    /// @brief Try to lock data for read-only access.
    /// @return Handle on owned data if available. Check before use.
    inline auto try_lock() const -> decltype(std::declval<Mustex>().try_lock_impl())
    {
        return try_lock_impl();
    }

    /// @brief Try to lock data for read-only access.
    /// @return Handle on owned data if available. Check before use.
    inline auto lock(std::try_to_lock_t) const -> decltype(std::declval<Mustex>().try_lock_impl())
    {
        return try_lock_impl();
    }

    /// @brief Try to lock data for read-only access for given amount of time.
    /// @param d Amount of time to try acquiring access. Returns if exceeded.
    /// @return Handle on owned data if available during given amount of time. Check before use.
    template<typename Rep, typename Period>
    inline auto try_lock_for(const std::chrono::duration<Rep, Period> &d) const
        -> decltype(std::declval<Mustex>().try_lock_for_impl(d))
    {
        return try_lock_for_impl(d);
    }

    /// @brief Try to lock data for read-only access until given instant is reached.
    /// @param tp Deadline for access to be granted. Returns if reached.
    /// @return Handle on owned data if available before deadline. Check before use.
    template<typename Clock, typename Duration>
    inline auto try_lock_until(const std::chrono::time_point<Clock, Duration> &tp) const
        -> decltype(std::declval<Mustex>().try_lock_until_impl(tp))
    {
        return try_lock_until_impl(tp);
    }

    /// @brief Lock data for write access.
    /// @return Handle on owned data.
    HandleMut lock_mut()
    {
        detail::proxy_mutex::lock_write(m_mutex);
        return HandleMut(&m_mutex, &m_data);
    }

    /// @brief Try to lock data for write access.
    /// @return Handle on owned data if available. Check before use.
    inline auto try_lock_mut() -> decltype(std::declval<Mustex>().try_lock_mut_impl())
    {
        return try_lock_mut_impl();
    }

    /// @brief Try to lock data for write access.
    /// @return Handle on owned data if available. Check before use.
    inline auto lock_mut(std::try_to_lock_t) -> decltype(std::declval<Mustex>().try_lock_mut_impl())
    {
        return try_lock_mut_impl();
    }

    /// @brief Try to lock data for write access for given amount of time.
    /// @param d Amount of time to try acquiring access. Returns if exceeded.
    /// @return Handle on owned data if available during given amount of time. Check before use.
    template<typename Rep, typename Period>
    inline auto try_lock_mut_for(const std::chrono::duration<Rep, Period> &d)
        -> decltype(std::declval<Mustex>().try_lock_mut_for_impl(d))
    {
        return try_lock_mut_for_impl(d);
    }

    /// @brief Try to lock data for write access until given instant is reached.
    /// @param tp Deadline for access to be granted. Returns if reached.
    /// @return Handle on owned data if available before deadline. Check before use.
    template<typename Clock, typename Duration>
    inline auto try_lock_mut_until(const std::chrono::time_point<Clock, Duration> &tp)
        -> decltype(std::declval<Mustex>().try_lock_mut_until_impl(tp))
    {
        return try_lock_mut_until_impl(tp);
    }

private:
    T m_data;
    mutable M m_mutex;

    // These are necessary in order for bcx::lock_mut to work.
    template<typename U>
    friend auto detail::get_mutex_ref(U &m) -> typename std::enable_if<detail::is_mustex<U>::value, typename U::mutex_t &>::type;
    template<template<class> class _WL, typename U>
    friend auto detail::adopt_lock(U &m) -> typename std::enable_if<detail::is_mustex<U>::value, typename U::HandleMut>::type;
    HandleMut lock_mut(std::adopt_lock_t) { return HandleMut(&m_mutex, &m_data); }
};
} // namespace bcx

#endif // #ifndef BCX_MUSTEX_HPP