#pragma once

#include <cassert>
#include <functional>
#include <list>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#define MOVE_ONLY(TYPE)                    \
    TYPE(TYPE const&) = delete;            \
    TYPE& operator=(TYPE const&) = delete; \
    TYPE(TYPE&&) = default;                \
    TYPE& operator=(TYPE&&) = default

#define ALL_FRIEND                    \
    template <typename X, typename Y> \
    friend class AdapterBase;         \
    template <typename X>             \
    friend class Iterator;            \
    template <typename X, typename Y> \
    friend class Chain;               \
    template <typename X>             \
    friend class Enumerate;           \
    template <typename X, typename Y> \
    friend class Filter;              \
    template <typename X, typename Y> \
    friend class Map;                 \
    template <typename X>             \
    friend class Reverse;             \
    template <typename X>             \
    friend class Skip;                \
    template <typename X>             \
    friend class StepBy;              \
    template <typename X>             \
    friend class Take;                \
    template <typename X, typename Y> \
    friend class Zip;

namespace detail
{
template <typename T, typename = void>
constexpr bool Iterable = false;

template <typename T>
constexpr bool Iterable<
    T,
    std::void_t<
        typename T::value_type,
        typename T::iterator,
        typename T::const_iterator,
        decltype(std::declval<T>().begin()),
        decltype(std::declval<T>().end()),
        decltype(std::declval<T>().cbegin()),
        decltype(std::declval<T>().cend())>> = true;

template <typename IterT>
constexpr bool IsBidirectionalV =
    std::is_base_of_v<std::bidirectional_iterator_tag, typename std::iterator_traits<IterT>::iterator_category>;

template <typename T>
struct Emplacer final {
    template <typename... Args>
    static void emplace(T& container, Args&&... args)
    {
        container.emplace(std::forward<Args>(args)...);
    }
};

template <typename T>
struct Emplacer<std::vector<T>> final {
    template <typename U>
    static void emplace(std::vector<T>& vec, U&& value)
    {
        vec.emplace_back(std::forward<U>(value));
    }
};

template <typename T>
struct Emplacer<std::list<T>> final {
    template <typename U>
    static void emplace(std::list<T>& list, U&& value)
    {
        list.emplace_back(std::forward<U>(value));
    }
};

template <typename T>
class [[nodiscard]] Iterator;

template <typename T, typename U>
class [[nodiscard]] Chain;

template <typename T>
class [[nodiscard]] Enumerate;

template <typename T, typename FnT>
class [[nodiscard]] Filter;

template <typename T, typename FnT>
class [[nodiscard]] Map;

template <typename T>
class [[nodiscard]] Reverse;

template <typename T>
class [[nodiscard]] Skip;

template <typename T>
class [[nodiscard]] StepBy;

template <typename T>
class [[nodiscard]] Take;

template <typename T, typename U>
class [[nodiscard]] Zip;

template <typename T, typename IterT>
class [[nodiscard]] IterPair final {
public:
    MOVE_ONLY(IterPair);

    ALL_FRIEND;

    using value_type = decltype(*std::declval<IterT>());
    using std_iterator = IterT;
    using mut_or_const_iterator = IterT;

    static_assert(std::is_reference_v<value_type>);

    explicit IterPair(T const& t)
    : m_iter(t.begin())
    , m_end(t.end())
    {
    }

    explicit IterPair(T& t)
    : m_iter(t.begin())
    , m_end(t.end())
    {
    }

    explicit IterPair(T&& t) = delete;

    value_type operator*() { return next(); }

private:
    bool empty() const { return m_iter == m_end; }

    size_t distance() const { return static_cast<size_t>(std::distance(m_iter, m_end)); }

    value_type get() { return *m_iter; }

    value_type get_back()
    {
        if constexpr (IsBidirectionalV<IterT>) {
            auto copy = m_end;
            return *(--copy);
        }
        else {
            return *m_end;
        }
    }

    value_type next()
    {
        value_type item = *m_iter;
        ++m_iter;
        return item;
    }

    value_type next_back()
    {
        if constexpr (IsBidirectionalV<IterT>) {
            --m_end;
        }
        return *m_end;
    }

    void stop_iteration() { m_iter = m_end; }

    mut_or_const_iterator m_iter;
    mut_or_const_iterator m_end;
};

class Adapter {
protected:
    ~Adapter() = default;
};

template <typename T, typename AdapterT>
class [[nodiscard]] AdapterBase : public Adapter {
public:
    MOVE_ONLY(AdapterBase);

    explicit AdapterBase(T&& t)
    : m_iter(std::move(t))
    {
    }

    /// iterators
    AdapterT begin() { return cbegin(); }

    AdapterT begin() const { return cbegin(); }

    AdapterT cbegin() const
    {
        auto& self = const_cast<AdapterT&>(downcast());
        AdapterT clone = std::move(self);
        self.stop_iteration();
        return clone;
    }

    int end() { return 0; }

    int end() const { return 0; }

    int cend() const { return 0; }

    /// accessors
    AdapterBase& operator++() { return *this; }

    bool operator!=(int) const { return !downcast().empty(); }

    /// adapters
    template <typename FnT>
    [[nodiscard]] bool all(FnT const& fn)
    {
        return any_or_all(fn, false);
    }

    template <typename FnT>
    [[nodiscard]] bool any(FnT const& fn)
    {
        return any_or_all(fn, true);
    }

    template <typename U>
    Chain<AdapterT, U> chain(U&& u)
    {
        return Chain<AdapterT, U>(std::move(downcast()), std::forward<U>(u));
    }

    template <typename ContainerT>
    [[nodiscard]] ContainerT collect()
    {
        ContainerT container{};
        for (auto& self = downcast(); !self.empty();) {
            Emplacer<ContainerT>::emplace(container, self.next());
        }
        return container;
    }

    [[nodiscard]] size_t count()
    {
        size_t i = 0;
        for (auto& self = downcast(); !self.empty(); self.next()) {
            ++i;
        }
        return i;
    }

    Enumerate<AdapterT> enumerate() { return Enumerate<AdapterT>(std::move(downcast())); }

    template <typename FnT>
    Filter<AdapterT, FnT> filter(FnT&& fn)
    {
        return Filter<AdapterT, FnT>(std::move(downcast()), std::forward<FnT>(fn));
    }

    template <typename FnT>
    [[nodiscard]] auto find(FnT const& fn) /* -> std::optional<value_type> */
    {
        advance_while(fn, false);
        return fallible_deref();
    }

    template <typename InitT, typename FnT>
    [[nodiscard]] InitT fold(InitT const& init, FnT const& fn)
    {
        InitT result = init;
        for (auto& self = downcast(); !self.empty();) {
            result = fn(result, self.next());
        }
        return result;
    }

    template <typename FnT>
    void for_each(FnT const& fn)
    {
        for (auto& self = downcast(); !self.empty();) {
            fn(self.next());
        }
    }

    [[nodiscard]] auto last() /* -> std::optional<value_type> */
    {
        for (auto& self = downcast();;) {
            auto result = fallible_deref();
            if (!self.empty()) {
                self.next();
            }
            if (self.empty()) {
                return result;
            }
        }
    }

    template <typename FnT>
    Map<AdapterT, FnT> map(FnT&& fn)
    {
        return Map<AdapterT, FnT>(std::move(downcast()), std::forward<FnT>(fn));
    }

    [[nodiscard]] auto nth(size_t n) /* -> std::optional<value_type> */
    {
        advance_by(n);
        return fallible_deref();
    }

    template <typename RetT, typename FnT>
    [[nodiscard]] std::pair<RetT /* trues */, RetT /* falses */> partition(FnT const& fn)
    {
        std::pair<RetT, RetT> pair;
        for (auto& self = downcast(); !self.empty();) {
            auto& item = self.next();
            if (fn(item)) {
                Emplacer<RetT>::emplace(pair.first, item);
            }
            else {
                Emplacer<RetT>::emplace(pair.second, item);
            }
        }
        return pair;
    }

    template <typename FnT>
    [[nodiscard]] std::optional<size_t> position(FnT const& fn)
    {
        size_t num_steps = advance_while(fn, false);
        return (downcast().empty()) ? std::nullopt : std::optional<size_t>(num_steps);
    }

    Reverse<AdapterT> reverse() { return Reverse<AdapterT>(std::move(downcast())); }

    Skip<AdapterT> skip(size_t n) { return Skip<AdapterT>(std::move(downcast()), n); }

    StepBy<AdapterT> step_by(size_t step) { return StepBy<AdapterT>(std::move(downcast()), step); }

    Take<AdapterT> take(size_t n) { return Take<AdapterT>(std::move(downcast()), n); }

    template <typename U>
    Zip<AdapterT, U> zip(U&& u)
    {
        return Zip<AdapterT, U>(std::move(downcast()), std::forward<U>(u));
    }

protected:
    using std_iterator = typename T::std_iterator;

    ~AdapterBase() = default;

    /* virtual */ typename T::value_type get() { return this->m_iter.get(); }

    /* virtual */ typename T::value_type get_back() { return this->m_iter.get_back(); }

    /* virtual */ typename T::value_type next() { return this->m_iter.next(); }

    /* virtual */ typename T::value_type next_back() { return this->m_iter.next_back(); }

    /* virtual */ bool empty() const { return m_iter.empty(); }

    /* virtual */ void stop_iteration() { m_iter.stop_iteration(); }

    /* virtual */ size_t distance() const { return m_iter.distance(); }

    size_t advance_by(size_t n)
    {
        size_t num_steps = 0;
        auto& self = downcast();
        while ((!self.empty()) && (num_steps < n)) {
            self.next();
            ++num_steps;
        }
        return num_steps;
    }

    template <typename FnT>
    size_t advance_while(FnT const& fn, bool expected)
    {
        size_t num_steps = 0;
        auto& self = downcast();
        while ((!self.empty()) && (fn(self.get()) == expected)) {
            self.next();
            ++num_steps;
        }
        return num_steps;
    }

    template <typename FnT>
    size_t advance_back_while(FnT const& fn, bool expected)
    {
        size_t num_steps = 0;
        auto& self = downcast();
        while ((!self.empty()) && (fn(self.get_back()) == expected)) {
            self.next_back();
            ++num_steps;
        }
        return num_steps;
    }

    T m_iter;

private:
    template <typename FnT>
    bool any_or_all(FnT const& fn, bool b)
    {
        for (auto& self = downcast(); !self.empty();) {
            if (fn(self.next()) == b) {
                return b;
            }
        }
        return !b;
    }

    auto fallible_deref() /* -> std::optional<value_type> */
    {
        using value_type = decltype(downcast().next());
        if constexpr (std::is_reference_v<value_type>) {
            using return_type = std::optional<std::reference_wrapper<std::remove_reference_t<value_type>>>;
            return (downcast().empty()) ? return_type{} : return_type{std::reference_wrapper(downcast().get())};
        }
        else {
            using return_type = std::optional<value_type>;
            return (downcast().empty()) ? return_type{} : return_type{downcast().get()};
        }
    }

    inline AdapterT& downcast() { return const_cast<AdapterT&>(std::as_const(*this).downcast()); }

    inline AdapterT const& downcast() const { return *static_cast<AdapterT const*>(this); }
};

template <typename T>
class [[nodiscard]] Iterator final : public AdapterBase<T, Iterator<T>> {
public:
    MOVE_ONLY(Iterator);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    static_assert(std::is_reference_v<value_type>);

    explicit Iterator(T&& t)
    : AdapterBase<T, Iterator<T>>(std::move(t))
    {
    }

    value_type operator*() { return this->m_iter.next(); }
};

template <typename T, typename U>
class [[nodiscard]] Chain final : public AdapterBase<T, Chain<T, U>> {
public:
    MOVE_ONLY(Chain);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    static_assert(std::is_base_of_v<Adapter, U>, "Adapter required");
    static_assert(
        std::is_same_v<typename T::value_type, typename U::value_type>, "Chained adapter must return the same value type");

    explicit Chain(T&& t, U&& u)
    : AdapterBase<T, Chain<T, U>>(std::move(t))
    , m_chainedIter(std::move(u))
    {
    }

    value_type operator*() { return next(); }

private:
    bool empty() const /* override */ { return this->m_iter.empty() && m_chainedIter.empty(); }

    void stop_iteration() /* override */
    {
        this->m_iter.stop_iteration();
        m_chainedIter.stop_iteration();
    }

    size_t distance() const /* override */ { return this->m_iter.distance() + m_chainedIter.distance(); }

    value_type get() /* override */ { return (!this->m_iter.empty()) ? this->m_iter.get() : m_chainedIter.get(); }

    value_type get_back() /* override */
    {
        return (!this->m_chainedIter.empty()) ? m_chainedIter.get_back() : this->m_iter.get_back();
    }

    value_type next() /* override */ { return (!this->m_iter.empty()) ? this->m_iter.next() : m_chainedIter.next(); }

    value_type next_back() /* override */
    {
        return (!this->m_chainedIter.empty()) ? m_chainedIter.next_back() : this->m_iter.next_back();
    }

    U m_chainedIter;
};

template <typename T>
class [[nodiscard]] Enumerate final : public AdapterBase<T, Enumerate<T>> {
public:
    MOVE_ONLY(Enumerate);

    ALL_FRIEND;

    using value_type = std::pair<size_t, typename T::value_type>;

    explicit Enumerate(T&& t)
    : AdapterBase<T, Enumerate<T>>(std::move(t))
    {
    }

    value_type operator*() { return next(); }

private:
    value_type get() /* override */ { return {m_i, this->m_iter.get()}; }

    value_type get_back() /* override */
    {
        typename T::value_type item = this->m_iter.get_back();
        size_t i = m_i + this->distance();
        return {i, item};
    }

    value_type next() /* override */ { return {m_i++, this->m_iter.next()}; }

    value_type next_back() /* override */
    {
        typename T::value_type item = this->m_iter.next_back();
        size_t i = m_i + this->distance();
        return {i, item};
    }

    size_t m_i = 0;
};

template <typename T, typename FnT>
class [[nodiscard]] Filter final : public AdapterBase<T, Filter<T, FnT>> {
public:
    MOVE_ONLY(Filter);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    static_assert(std::is_invocable_v<FnT, typename T::value_type>, "Invocable required");
    static_assert(std::is_convertible_v<std::invoke_result_t<FnT, typename T::value_type>, bool>, "Predicate must return bool");

    Filter(T&& t, FnT&& fn)
    : AdapterBase<T, Filter<T, FnT>>(std::move(t))
    , m_predicate(std::forward<FnT>(fn))
    {
        this->m_iter.advance_while(m_predicate, false);
        this->m_iter.advance_back_while(m_predicate, false);
    }

    value_type operator*() { return next(); }

private:
    value_type next() /* override */
    {
        value_type item = this->m_iter.next();
        this->m_iter.advance_while(m_predicate, false);
        return item;
    }

    value_type next_back() /* override */
    {
        value_type item = this->m_iter.next_back();
        this->m_iter.advance_back_while(m_predicate, false);
        return item;
    }

    FnT m_predicate;
};

template <typename T, typename FnT>
class [[nodiscard]] Map final : public AdapterBase<T, Map<T, FnT>> {
public:
    MOVE_ONLY(Map);

    ALL_FRIEND;

    using value_type = std::invoke_result_t<FnT, typename T::value_type>;

    static_assert(std::is_invocable_v<FnT, typename T::value_type>, "Invocable required");

    Map(T&& t, FnT&& fn)
    : AdapterBase<T, Map<T, FnT>>(std::move(t))
    , m_f(std::forward<FnT>(fn))
    {
    }

    value_type operator*() { return next(); }

private:
    value_type get() /* override */ { return m_f(this->m_iter.get()); }

    value_type get_back() /* override */ { return m_f(this->m_iter.get_back()); }

    value_type next() /* override */ { return m_f(this->m_iter.next()); }

    value_type next_back() /* override */ { return m_f(this->m_iter.next_back()); }

    FnT m_f;
};

template <typename T>
class [[nodiscard]] Reverse final : public AdapterBase<T, Reverse<T>> {
public:
    MOVE_ONLY(Reverse);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    static_assert(
        IsBidirectionalV<typename AdapterBase<T, Reverse<T>>::std_iterator>, "Only bidirectional iterators can be reversed");

    Reverse(T&& t)
    : AdapterBase<T, Reverse<T>>(std::move(t))
    {
    }

    value_type operator*() { return next(); }

private:
    value_type get() /* override */ { return this->m_iter.get_back(); }

    value_type get_back() /* override */ { return this->m_iter.get(); }

    value_type next() /* override */ { return this->m_iter.next_back(); }

    value_type next_back() /* override */ { return this->m_iter.next(); }
};

template <typename T>
class [[nodiscard]] Skip final : public AdapterBase<T, Skip<T>> {
public:
    MOVE_ONLY(Skip);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    Skip(T&& t, size_t n)
    : AdapterBase<T, Skip<T>>(std::move(t))
    {
        this->advance_by(n);
    }

    value_type operator*() { return this->m_iter.next(); }
};

template <typename T>
class [[nodiscard]] StepBy final : public AdapterBase<T, StepBy<T>> {
public:
    MOVE_ONLY(StepBy);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    StepBy(T&& t, size_t step)
    : AdapterBase<T, StepBy<T>>(std::move(t))
    , m_step(step)
    {
        assert(step != 0);
    }

    value_type operator*() { return next(); }

private:
    value_type next() /* override */
    {
        value_type item = this->m_iter.next();
        for (size_t i = 1; (!this->empty()) && (i < get_step()); ++i) {
            this->m_iter.next();
        }
        return item;
    }

    value_type next_back() /* override */
    {
        initial_step_back();
        value_type item = this->m_iter.next_back();
        for (size_t i = 1; (!this->empty()) && (i < get_step()); ++i) {
            this->m_iter.next_back();
        }
        return item;
    }

    size_t get_step() const { return m_step & (static_cast<size_t>(-1) >> 1); }

    bool get_flag() const { return m_step & ~(static_cast<size_t>(-1) >> 1); }

    void initial_step_back()
    {
        if (get_flag()) {
            return;
        }

        m_step |= ~(static_cast<size_t>(-1) >> 1);
        size_t const initial_steps = (this->distance() - 1) % get_step();
        for (size_t i = 0; (i < initial_steps) && (!this->empty()); ++i) {
            this->m_iter.next_back();
        }
    }

    size_t m_step;
};

template <typename T>
class [[nodiscard]] Take final : public AdapterBase<T, Take<T>> {
public:
    MOVE_ONLY(Take);

    ALL_FRIEND;

    using value_type = typename T::value_type;

    Take(T&& t, size_t n)
    : AdapterBase<T, Take<T>>(std::move(t))
    , m_n(n)
    {
        if (m_n == 0) {
            this->stop_iteration();
        }
    }

    value_type operator*() { return next(); }

private:
    value_type next() /* override */
    {
        value_type item = this->m_iter.next();
        if (--m_n == 0) {
            this->stop_iteration();
        }
        return item;
    }

    value_type next_back() /* override */
    {
        value_type item = this->m_iter.next_back();
        if (--m_n == 0) {
            this->stop_iteration();
        }
        return item;
    }

    size_t m_n;
};

template <typename T, typename U>
class [[nodiscard]] Zip final : public AdapterBase<T, Zip<T, U>> {
public:
    MOVE_ONLY(Zip);

    ALL_FRIEND;

    using value_type = std::pair<typename T::value_type, typename U::value_type>;

    static_assert(std::is_base_of_v<Adapter, U>, "Adapter required");

    explicit Zip(T&& t, U&& u)
    : AdapterBase<T, Zip<T, U>>(std::move(t))
    , m_zippedIter(std::move(u))
    {
    }

    value_type operator*() { return next(); }

private:
    bool empty() const /* override */ { return this->m_iter.empty() || m_zippedIter.empty(); }

    size_t distance() const /* override */ { return std::min(this->m_iter.distance(), m_zippedIter.distance()); }

    value_type get() /* override */ { return {this->m_iter.get(), m_zippedIter.get()}; }

    value_type get_back() /* override */ { return {this->m_iter.get_back(), m_zippedIter.get_back()}; }

    value_type next() /* override */ { return {this->m_iter.next(), m_zippedIter.next()}; }

    value_type next_back() /* override */ { return {this->m_iter.next_back(), m_zippedIter.next_back()}; }

    U m_zippedIter;
};

template <typename IterT, typename T>
auto mut_or_const_iter(T* t)
{
    assert(t != nullptr);
    using PlainT = std::decay_t<T>;
    using Pair = IterPair<PlainT, IterT>;
    static_assert(Iterable<PlainT>, "Iterable required");
    return Iterator<Pair>(Pair(*t));
}
} // namespace detail

template <typename T>
auto iter(T const* t)
{
    return detail::mut_or_const_iter<typename T::const_iterator>(t);
}

template <typename T>
auto iter_mut(T* t)
{
    return detail::mut_or_const_iter<typename T::iterator>(t);
}

#undef MOVE_ONLY
#undef ALL_FRIEND
