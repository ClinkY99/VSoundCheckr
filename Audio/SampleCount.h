//
// Created by cline on 2025-03-08.
//

#ifndef SAMPLECOUNT_H
#define SAMPLECOUNT_H
#include <limits>
#include <cstddef>


//audio positions require a wide format, this disallows conversions to smaller types
class sampleCount {
public:
    using type = long long;
private:
    type value;

public:
    sampleCount () : value { 0 } {}

    // Allow implicit conversion from integral types
    sampleCount ( type v ) : value { v } {}
    sampleCount ( unsigned long long v ) : value ( v ) {}
    sampleCount ( int v ) : value { v } {}
    sampleCount ( unsigned v ) : value { v } {}
    sampleCount ( long v ) : value { v } {}

    // unsigned long is 64 bit on some platforms.  Let it narrow.
    sampleCount ( unsigned long v ) : value ( v ) {}

    // Beware implicit conversions from floating point values!
    // Otherwise the meaning of binary operators with sampleCount change
    // their meaning when sampleCount is not an alias!
    explicit sampleCount ( float f ) : value ( f ) {}
    explicit sampleCount ( double d ) : value ( d ) {}

    sampleCount ( const sampleCount& ) = default;
    sampleCount &operator= ( const sampleCount& ) = default;

    float as_float() const { return value; }
    double as_double() const { return value; }

    long long as_long_long() const { return value; }

    size_t as_size_t() const;

    sampleCount &operator += (sampleCount b) { value += b.value; return *this; }
    sampleCount &operator -= (sampleCount b) { value -= b.value; return *this; }
    sampleCount &operator *= (sampleCount b) { value *= b.value; return *this; }
    sampleCount &operator /= (sampleCount b) { value /= b.value; return *this; }
    sampleCount &operator %= (sampleCount b) { value %= b.value; return *this; }

    sampleCount operator - () const { return -value; }

    sampleCount &operator ++ () { ++value; return *this; }
    sampleCount operator ++ (int)
    { sampleCount result{ *this }; ++value; return result; }

    sampleCount &operator -- () { --value; return *this; }
    sampleCount operator -- (int)
    { sampleCount result{ *this }; --value; return result; }

    static sampleCount min() { return std::numeric_limits<type>::min(); }
    static sampleCount max() { return std::numeric_limits<type>::max(); }
};

inline bool operator == (sampleCount a, sampleCount b)
{
    return a.as_long_long() == b.as_long_long();
}

inline bool operator != (sampleCount a, sampleCount b)
{
    return !(a == b);
}

inline bool operator < (sampleCount a, sampleCount b)
{
    return a.as_long_long() < b.as_long_long();
}

inline bool operator >= (sampleCount a, sampleCount b)
{
    return !(a < b);
}

inline bool operator > (sampleCount a, sampleCount b)
{
    return b < a;
}

inline bool operator <= (sampleCount a, sampleCount b)
{
    return !(b < a);
}

inline sampleCount operator + (sampleCount a, sampleCount b)
{
    return sampleCount{ a } += b;
}

inline sampleCount operator - (sampleCount a, sampleCount b)
{
    return sampleCount{ a } -= b;
}

inline sampleCount operator * (sampleCount a, sampleCount b)
{
    return sampleCount{ a } *= b;
}

inline sampleCount operator / (sampleCount a, sampleCount b)
{
    return sampleCount{ a } /= b;
}

inline sampleCount operator % (sampleCount a, sampleCount b)
{
    return sampleCount{ a } %= b;
}



#endif //SAMPLECOUNT_H
