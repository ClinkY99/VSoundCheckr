//
// Created by cline on 2025-02-20.
//

#ifndef MEMORYX_H
#define MEMORYX_H
#include <algorithm>
#include <memory>


class MemoryX {

};


template <typename T> struct NonInterfering : T {
    using T::T;

    //functions IDK if I need these

    void set(const T &other) {
        T::operator=(other);
    }
    void set(T &&other) {
        T::operator = (std::move(other));
    }
};


template <typename T> class ArrayOf
    : public std::unique_ptr<T[]> {
public:

    ArrayOf() {
    }

    template <typename Integral>
    ArrayOf(Integral count, bool initialize =false) {
        static_assert(std::is_unsigned<Integral>::value, "Unsigned values only");
        Reinit(count, initialize);
    }

    ArrayOf(ArrayOf&& that) {
        std::unique_ptr<T[]> (std::move(that));
    }

    ArrayOf& operator= (ArrayOf&& that) {
        std::unique_ptr<T[]>::operator=(std::move(that));
        return *this;
    }

    ArrayOf& operator= (std::unique_ptr<T[]>&& that) {
        std::unique_ptr<T[]>::operator=(std::move(that));
        return *this;
    }

    template <typename Integral>
    void Reinit(Integral count, bool initialize = false) {
        static_assert(std::is_unsigned<Integral>::value, "Unsigned values only");

        if (initialize) {
            std::unique_ptr<T[]>::reset(new T[count] {});
        } else {
            std::unique_ptr<T[]>::reset(new T[count]);
        }
    }
};

// Construct this from any copyable function object, such as a lambda
template <typename F>
struct Finally {
    Finally(F f) : clean( f ) {}
    ~Finally() { clean(); }
    F clean;
};

template <typename F>
[[nodiscard]] Finally<F> finally (F f)
{
    return Finally<F>(f);
}

using Floats = ArrayOf<float>;


#endif //MEMORYX_H
