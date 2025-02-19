#ifndef BCX_MUSTEX_HPP
#define BCX_MUSTEX_HPP

#if __cplusplus >= 202002L
#    include <concepts>
#endif // #if __cplusplus >= 202002L

#if __cplusplus >= 201103L && __cplusplus < 201703L
#    include <mutex>
#elif __cplusplus >= 201703L
#    include <optional>
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
#if __cplusplus >= 202002L
        // Prevent from using this constructor when argument is a musted (ref or moved)
        requires(!std::is_same_v<Mustex, std::remove_cvref_t<Args>> && ...)
#endif // #if __cplusplus >= 202002L
    Mustex(Args &&...args)
        : m_data(std::forward<Args>(args)...)
        , m_mutex{}
    {
    }

#if __cplusplus >= 202002L
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
        m_data = *other.lock();
    }
    Mustex &operator=(Mustex &&other)
        requires std::is_assignable<T, T &&>::value
    {
        m_data = std::move(*other.lock_mut());
    }
#else // #if __cplusplus >= 202002L
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
#endif // #if __cplusplus < 202002L

    virtual ~Mustex() = default;

#if __cplusplus >= 201703L
    MustexHandle<const T, std::shared_lock<MustexMutexType>> lock() const
    {
        return MustexHandle<const T, std::shared_lock<MustexMutexType>>(std::ref(m_mutex), m_data);
    }

    std::optional<MustexHandle<const T, std::shared_lock<MustexMutexType>>> try_lock() const
    {
        std::shared_lock lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return MustexHandle<const T, std::shared_lock<MustexMutexType>>(std::move(lock), m_data);
        return {};
    }

    std::optional<MustexHandle<T, std::unique_lock<MustexMutexType>>> try_lock_mut()
    {
        std::unique_lock lock(m_mutex, std::try_to_lock);
        if (lock.owns_lock())
            return MustexHandle<T, std::unique_lock<MustexMutexType>>(std::move(lock), m_data);
        return {};
    }
#endif // #if __cplusplus >= 201703L

    MustexHandle<T, std::unique_lock<MustexMutexType>> lock_mut()
    {
        return MustexHandle<T, std::unique_lock<MustexMutexType>>(std::ref(m_mutex), m_data);
    }

private:
    T m_data;
    mutable MustexMutexType m_mutex;
};
} // namespace bcx

#endif // #ifndef BCX_MUSTEX_HPP