/*
 *  Copyright (C) 2015-2019 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */

#pragma once

#include <utility> // std::swap
#include <cstdlib> // std::abs
#include <iostream>
#include <cmath> // std::fmod
#include <functional> // std::modulus
#include <ciso646> // and, or ...

extern "C" {
#include <libavutil/rational.h> // specify conversions for AVRational
}

namespace jami {

/**
 * Naive implementation of the boost::rational interface, described here:
 * http://www.boost.org/doc/libs/1_57_0/libs/rational/rational.html
 */
template<typename I>
class rational {
public:
    // Constructors
    rational() {};          // Zero
    rational(I n) : num_(n) {};       // Equal to n/1
    rational(I n, I d) : num_(n), den_(d) {
        reduce();
    };  // General case (n/d)

    // Define conversions to and from AVRational (equivalent)
    rational(AVRational r) : num_(r.num), den_(r.den) {};
    operator AVRational() const { return AVRational{(int)num_, (int)den_}; }

    // Normal copy constructors and assignment operators

    // Assignment from I
    rational& operator=(I n) { num_ = n; den_ = 1; return *this; }

    // Assign in place
    rational& assign(I n, I d) { num_ = n; den_ = d; reduce(); return *this; }

    // Representation
    I numerator() const { return num_; };
    I denominator() const { return den_; };
    template<typename R=double> R real() const { return num_/(R)den_; }

    // In addition to the following operators, all of the "obvious" derived
    // operators are available - see operators.hpp

    // Arithmetic operators
    rational operator+ (const rational& r) const {
        return {num_*r.den_ + r.num_*den_, den_*r.den_};
    }
    rational operator- (const rational& r) const {
        return {num_*r.den_ - r.num_*den_, den_*r.den_};
    }
    rational operator* (const rational& r) const {
        return {num_*r.num_, den_*r.den_};
    }
    rational operator/ (const rational& r) const {
        return {num_*r.den_, den_*r.num_};
    }

    rational& operator+= (const rational& r) {
        std::swap(*this, *this + r);
        return *this;
    }
    rational& operator-= (const rational& r) {
        std::swap(*this, *this - r);
        return *this;
    }
    rational& operator*= (const rational& r) {
        num_ *= r.num_;
        den_ *= r.den_;
        reduce();
        return *this;
    }
    rational& operator/= (const rational& r) {
        num_ *= r.den_;
        den_ *= r.num_;
        reduce();
        return *this;
    }

    // Arithmetic with integers
    rational& operator+= (I i) { num_ += i * den_; return *this; }
    rational& operator-= (I i) { num_ -= i * den_; return *this; }
    rational& operator*= (I i) { num_ *= i; reduce(); return *this; }
    rational& operator/= (I i) { den_ *= i; reduce(); return *this; }

    // Increment and decrement
    const rational& operator++() { num_ += den_; return *this; }
    const rational& operator--() { num_ -= den_; return *this; }

    // Operator not
    bool operator!() const { return !num_; };

    // Boolean conversion
    explicit operator bool() const { return num_; }

    // Comparison operators
    bool operator< (const rational& r) const {
        bool inv = (den_ > 0) != (r.den_ > 0);
        return inv != (num_ * r.den_ < den_ * r.num_);
    }
    bool operator> (const rational& r) const {
        bool inv = (den_ > 0) != (r.den_ > 0);
        return inv != (num_ * r.den_ > den_ * r.num_);
    }
    bool operator== (const rational& r) const { return num_ * r.den_ == den_ * r.num_; }
    bool operator!= (const rational& r) const { return num_ * r.den_ != den_ * r.num_; }

    // Comparison with integers
    bool operator< (I i) const { return den_ < 0 ? (num_ > i * den_) : (num_ < i * den_); }
    bool operator> (I i) const { return den_ < 0 ? (num_ < i * den_) : (num_ > i * den_); }
    bool operator== (I i) const { return num_ == i * den_; }
    bool operator!= (I i) const { return num_ != i * den_; }
private:
    I num_ {0};
    I den_ {1};

    static constexpr I gcd ( I a, I b ) {
        return b == (I)0 ? a : gcd(b, std::modulus<I>()(a, b));
    }
    void reduce() {
        if (std::is_integral<I>::value) {
            if (num_ and den_) {
                auto g = gcd(num_ >= 0 ? num_ : -num_, den_ >= 0 ? den_ : -den_);
                if (g > (I)1) {
                    num_ /= g;
                    den_ /= g;
                }
            }
        }
    }
};

// Unary operators
template <typename I> rational<I> operator+ (const rational<I>& r) { return r; }
template <typename I> rational<I> operator- (const rational<I>& r) { return {-r.numerator(), r.denominator()}; };

// Reversed order operators for - and / between (types convertible to) I and rational
template <typename I, typename II> inline rational<I> operator- (II i, const rational<I>& r);
template <typename I, typename II> inline rational<I> operator/ (II i, const rational<I>& r) {
    return { i * r.denominator(), r.numerator() };
}

// Absolute value
template <typename I> rational<I> abs (const rational<I>& r) {
    return { std::abs(r.numerator()), std::abs(r.denominator()) };
}

// Input and output
template <typename I> std::istream& operator>> (std::istream& is, rational<I>& r) {
    char sep;
    is >> r.num_ >> sep >> r.den_;
    return is;
}

template <typename I> std::ostream& operator<< (std::ostream& os, const rational<I>& r) {
    os << r.numerator() << '/' << r.denominator();
    return os;
}

// Type conversion
template <typename T, typename I> T rational_cast (const rational<I>& r);

}

namespace std {
  template <>
  struct modulus<double> {
    double operator()(const double &lhs, const double &rhs) const {
      return std::fmod(lhs, rhs);
    }
  };
} // namespace std
