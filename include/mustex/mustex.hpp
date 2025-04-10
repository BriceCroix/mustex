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

#include <mutex>
#include <utility>

namespace bcx
{

// Forward declares
template<typename T, class M, template<class> class RL, template<class> class WL>
class Mustex;

namespace detail
{
#ifdef _MUSTEX_HAS_SHARED_MUTEX
template<class M>
using DefaultMustexReadLock = std::shared_lock<M>;
using DefaultMustexMutex = std::shared_timed_mutex;
#else // #ifdef _MUSTEX_HAS_SHARED_MUTEX
template<class M>
using DefaultMustexReadLock = std::unique_lock<M>;
using DefaultMustexMutex = std::mutex;
#endif // #ifdef _MUSTEX_HAS_SHARED_MUTEX

template<class M>
using DefaultMustexWriteLock = std::unique_lock<M>;

template<typename U>
struct is_mustex : std::false_type
{
};

template<typename T, class M, template<class> class RL, template<class> class WL>
struct is_mustex<Mustex<T, M, RL, WL>> : std::true_type
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
template<template<class> class WL, typename T>
auto adopt_lock(T &m) -> typename std::enable_if<!is_mustex<T>::value, DefaultMustexWriteLock<T>>::type
{
    return WL<T>(m, std::adopt_lock);
}

/// @brief Acquire lock (adopt) for Mustex.
template<template<class> class WL, typename T>
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

template<template<class> class WL, typename... Args>
auto lock_mut_impl(Args &...args) -> std::tuple<decltype(detail::adopt_lock<WL>(args))...>
{
    auto mutex_refs = std::tie(detail::get_mutex_ref(args)...);
    detail::lock_all(mutex_refs);
    return std::make_tuple(detail::adopt_lock<WL>(args)...);
}

template<template<class> class WL, typename... Args>
auto try_lock_mut_impl(Args &...args)
#ifdef _MUSTEX_HAS_OPTIONAL
    -> std::optional<std::tuple<decltype(detail::adopt_lock<WL>(args))...>>
#else
    -> std::unique_ptr<std::tuple<decltype(detail::adopt_lock<WL>(args))...>>
#endif
{
    auto mutex_refs = std::tie(detail::get_mutex_ref(args)...);
    if (!detail::try_lock_all(mutex_refs))
        return {};
    auto tuple = std::make_tuple(detail::adopt_lock<WL>(args)...);
#ifdef _MUSTEX_HAS_OPTIONAL
    return std::move(tuple);
#else
    return std::unique_ptr<std::tuple<decltype(detail::adopt_lock<WL>(args))...>>(new auto(std::move(tuple)));
#endif
}

} // namespace detail

/// @brief Lock mutably any given Mustex or raw mutex using deadlock avoidance.
/// @tparam WL Type of lock to use on raw mutexes.
/// @tparam ...Args Types of given arguments.
/// @param ...args Arbitrary number of Mustex and/or raw mutex.
/// @return Tuple containing a Mustex::HandleMut for each given Mustex, and a lock for each raw mutex.
template<template<class> class WL = detail::DefaultMustexWriteLock, typename... Args>
auto lock_mut(Args &...args) -> decltype(detail::lock_mut_impl<WL>(args...))
{
    return detail::lock_mut_impl<WL>(args...);
}

/// @brief Tries to lock mutably any given Mustex or raw mutex using deadlock avoidance.
/// @tparam WL Type of lock to use on raw mutexes.
/// @tparam ...Args Types of given arguments.
/// @param ...args Arbitrary number of Mustex and/or raw mutex.
/// @return Optional tuple. Contains nothing if at least one argument could not be locked.
///         Otherwise contains a Mustex::HandleMut for each given Mustex, and a lock for each raw mutex.
template<template<class> class WL = detail::DefaultMustexWriteLock, typename... Args>
auto try_lock_mut(Args &...args) -> decltype(detail::try_lock_mut_impl<WL>(args...))
{
    return detail::try_lock_mut_impl<WL>(args...);
}

/// @brief Allow to access Mustex data, mutably or not depending on method used to construct.
/// @tparam T Type of data to be accessed, potentially const-qualified.
/// @tparam L Type of lock owned by this class.
template<typename T, class L>
class MustexHandle
{
public:
    // Only parent Mustex can instantiate this class.
    template<class MT, class MM, template<class> class MRL, template<class> class MWL>
    friend class Mustex;

    /// @brief The type of contained value, exposed for convenience.
    using data_t = typename std::remove_cv<T>::type;

    MustexHandle() = delete;
    MustexHandle(const MustexHandle &) = delete;
    MustexHandle(MustexHandle &&other)
        : m_lock{std::move(other.m_lock)}
        , m_data{other.m_data}
    {
    }

    MustexHandle &operator=(const MustexHandle &other) = delete;
    MustexHandle &operator=(MustexHandle &&other)
    {
        m_data = other.m_data;
        m_lock = std::move(other.m_lock);
        return *this;
    }

    virtual ~MustexHandle() = default;

    T &operator*()
    {
        return *m_data;
    }

    T *operator->()
    {
        return m_data;
    }

private:
    L m_lock;
    T *m_data;

    MustexHandle(L &&lock, T *data)
        : m_lock(std::move(lock))
        , m_data{data}
    {
    }
};

/// @brief Data-owning mutex class, allowing never to access shared data
/// without thread synchronization.
/// @tparam T The type of data to be shared among threads.
/// @tparam M Type of synchronization mutex.
/// @tparam RL Type of lock used for read-accesses.
/// @tparam WL Type of lock used for write-accesses.
template<
    class T,
    class M = detail::DefaultMustexMutex,
    template<class> class RL = detail::DefaultMustexReadLock,
    template<class> class WL = detail::DefaultMustexWriteLock>
class Mustex
{
public:
    /// @brief The type of contained value, exposed for convenience.
    using data_t = typename std::remove_cv<T>::type;
    /// @brief The type of mutex used, exposed for convenience.
    using mutex_t = M;
    /// @brief The type of handle used to access data.
    using Handle = MustexHandle<const data_t, RL<M>>;
    /// @brief The type of handle used to access data mutably.
    using HandleMut = MustexHandle<data_t, WL<M>>;

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
        WL<M> lock(m_mutex);
        m_data = *other.lock();
        return *this;
    }
    Mustex &operator=(Mustex &&other)
        requires std::is_assignable<T &, T &&>::value
    {
        // WL<M> lock(m_mutex);
        auto lock_and_handle = bcx::lock_mut<WL>(m_mutex, other);
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
    std::optional<Handle> try_lock_impl() const
    {
        RL<M> lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return Handle(std::move(lock), &m_data);
        return {};
    }
    std::optional<HandleMut> try_lock_mut_impl()
    {
        WL<M> lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return HandleMut(std::move(lock), &m_data);
        return {};
    }
#else // #ifdef _MUSTEX_HAS_OPTIONAL
    std::unique_ptr<Handle> try_lock_impl() const
    {
        RL<M> lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return std::unique_ptr<Handle>(
                new Handle(std::move(lock), &m_data)
            );
        return {};
    }
    std::unique_ptr<HandleMut> try_lock_mut_impl()
    {
        WL<M> lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return std::unique_ptr<HandleMut>(
                new HandleMut(std::move(lock), &m_data)
            );
        return {};
    }
#endif // #ifdef _MUSTEX_HAS_OPTIONAL
public:
    /// @brief Lock data for read-only access.
    /// @return Handle on owned data.
    Handle lock() const
    {
        return Handle(RL<M>(m_mutex), &m_data);
    }

    /// @brief Try to lock data for read-only access.
    /// @return Handle on owned data if available. Check before use.
    auto try_lock() const -> decltype(std::declval<Mustex>().try_lock_impl())
    {
        return try_lock_impl();
    }

    /// @brief Try to lock data for read-only access.
    /// @return Handle on owned data if available. Check before use.
    auto lock(std::try_to_lock_t) const -> decltype(std::declval<Mustex>().try_lock_impl())
    {
        return try_lock_impl();
    }

    /// @brief Lock data for write access.
    /// @return Handle on owned data.
    HandleMut lock_mut()
    {
        return HandleMut(WL<M>(m_mutex), &m_data);
    }

    /// @brief Try to lock data for write access.
    /// @return Handle on owned data if available. Check before use.
    auto try_lock_mut() -> decltype(std::declval<Mustex>().try_lock_mut_impl())
    {
        return try_lock_mut_impl();
    }

    /// @brief Try to lock data for write access.
    /// @return Handle on owned data if available. Check before use.
    auto lock_mut(std::try_to_lock_t) -> decltype(std::declval<Mustex>().try_lock_mut_impl())
    {
        return try_lock_mut_impl();
    }

private:
    T m_data;
    mutable M m_mutex;

    // These are necessary in order for bcx::lock_mut to work.
    template<typename U>
    friend auto detail::get_mutex_ref(U &m) -> typename std::enable_if<detail::is_mustex<U>::value, typename U::mutex_t &>::type;
    template<template<class> class _WL, typename U>
    friend auto detail::adopt_lock(U &m) -> typename std::enable_if<detail::is_mustex<U>::value, typename U::HandleMut>::type;
    HandleMut lock_mut(std::adopt_lock_t) { return HandleMut(WL<M>(m_mutex, std::adopt_lock), &m_data); }
};
} // namespace bcx

#endif // #ifndef BCX_MUSTEX_HPP