//
// Created by cline on 2025-02-20.
//

#ifndef MEMORYX_H
#define MEMORYX_H
#include <algorithm>


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


#endif //MEMORYX_H
