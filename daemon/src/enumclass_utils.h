/*
 *  Copyright (C) 2014 Savoir-Faire Linux Inc.
 *
 *  Author: Emmanuel Lepage <emmanuel.lepage@savoirfairelinux.com>
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
 *
 */
#ifndef ENUM_CLASS_UTILS_H
#define ENUM_CLASS_UTILS_H

#include <map>
#include "logger.h"
#include <type_traits>
#include <vector>
#include <cassert>

namespace sfl {

/**
 * This function add a safe way to get an enum class size
 * @note it cannot be "const" due to some compiler issues
 * @note it cannot be unsigned to avoid some compiler warnings
 */
template<typename A> constexpr inline int enum_class_size() {
   return size_t(A::__COUNT);
}

/**
 * This generic class is used to have multidimensional enum class array.
 * It safely convert them to integers. Each enum class need a "COUNT" item
 * at the end.
 *
 * This struct enforce:
 *  * That the row are indexed using enum_classes
 *  * That the size of the matrix match the enum_class size
 *  * That the operators are within the matrix boundary
 */
template<class Row, class Value, class A = Value>
struct Matrix1D
{

    Matrix1D(std::initializer_list< std::initializer_list<Value> > s);

    // Operators

    Value operator[](Row v);

    const Value operator[](Row v) const;

    Value begin();

    Value end();

    //Only use for single reverse mappable arrays, will ASSERT otherwise
    Row fromValue(const Value value) const;

    static void setReverseMapping(Matrix1D<Row,const char*> names);

private:
    const std::vector<Value> data_;
    static std::map<A,Row> reverseMapping_;
};


/**
 * A matrix with no value
 *
 * This is useful to use enum class in C++11 foreach loops
 *
 * @usage
 *   for (const MyEnum& value : sfl::Matrix0D<MyEnum>()) {
 *       std::cout << "Name: " << MyEnumNames[value] << std::endl;
 *   }
 */
template<class EnumClass >
struct Matrix0D
{

    /**
    * An Iterator for enum classes
    */
    class EnumClassIter
    {
    public:
        EnumClassIter (const Matrix0D<EnumClass>* p_vec, int pos)
            : pos_( pos ), p_vec_( p_vec ) {}

        bool operator!= (const EnumClassIter& other) const;
        EnumClass operator* () const;
        const EnumClassIter& operator++ ();

    private:
        int pos_;
        const Matrix0D<EnumClass> *p_vec_;
    };

    Matrix0D();

    //Iterators
    EnumClassIter begin();
    EnumClassIter end();
};

/**
 * A helper to type to match serializable string to enum elements
 */
template<class Row>
using EnumClassNames = Matrix1D<Row,const char*>;

/**
 * Create a matrix type with 2 enum class dimensions M[I,J] = V
 *                                                     ^ ^    ^
 *                                                     | |    |
 *                                          Rows    <--- |    |
 *                                          Columns <-----    |
 *                                          Value   <----------
 */
template<class Row, class Column, class Value>
using Matrix2D = Matrix1D<Row, Matrix1D<Column, Value>>;

/**
 * Create an array of callbacks.
 *
 * This type hide all the C++ syntax requirement
 */
template<class Row, class Class, typename Result = void,typename... Args>
using CallbackMatrix1D = Matrix1D<Row,Result(Class::*)(Args... args)>;

/**
 * Create a method callback matrix.
 *
 * This type hide all the C++ syntax requirement
 */
template<class Row, class Column, class Class, typename Result = void,typename... Args>
using CallbackMatrix2D = Matrix2D<Row,Column,void(Class::*)(Args... args)>;






/*
 * IMPLEMENTATION
 *
 * In C++11 theory, this could go into the .cpp, but GCC still have some
 * unresolved issues regarding this.
 */

template<class Row, class Value, class Accessor>
Matrix1D<Row,Value,Accessor>::Matrix1D(std::initializer_list< std::initializer_list<Value> > s)
: data_(*std::begin(s)) {
    static_assert(std::is_enum<Row>(),"Row has to be an enum class");

    // FIXME C++14, use static_assert and make the ctor constexpr
    assert(std::begin(s)->size() == enum_class_size<Row>());//,"Matrix row have to match the enum class size");
}

template<class Row, class Value, class Accessor>
Value Matrix1D<Row,Value,Accessor>::operator[](Row v) {
    //ASSERT(size_t(v) >= size_t(Row::__COUNT),"State Machine Out of Bound\n");
    if (size_t(v) >= enum_class_size<Row>() || static_cast<int>(v) < 0) {
        ERROR("State Machine Out of Bound %d\n", size_t(v));
        assert(false);
        throw v;
    }
    return data_[size_t(v)];
}

template<class Row, class Value, class Accessor>
const Value Matrix1D<Row,Value,Accessor>::operator[](Row v) const {
    assert(size_t(v) <= enum_class_size<Row>()+1 && size_t(v)>=0); //__COUNT is also valid
    if (size_t(v) >= enum_class_size<Row>()) {
        ERROR("State Machine Out of Bound %d\n", size_t(v));
        assert(false);
        throw v;
    }
    return data_[size_t(v)];
}

template <class E, class T, class A> std::map<A,E> Matrix1D<E,T,A>::reverseMapping_;

template<class Row, class Value, class Accessor>
void Matrix1D<Row,Value,Accessor>::setReverseMapping(Matrix1D<Row,const char*> names)
{
    for ( const Row row : sfl::Matrix0D<Row>() )
        reverseMapping_[names[row]] = row;
}

template<class Row, class Value, class Accessor>
Row Matrix1D<Row,Value,Accessor>::fromValue(const Value value) const {
    if (!reverseMapping_.size()) {
        for (int i =0;i<enum_class_size<Row>();i++) {
        const_cast<Matrix1D*>(this)->reverseMapping_[(*const_cast<Matrix1D*>(this))[(Row)i]]
            = static_cast<Row>(i);
        }
        assert(reverseMapping_.size() == enum_class_size<Row>());
    }
    if (reverseMapping_.count(value) == 0) {
        throw value;
    }
    return reverseMapping_[value];
}

template<class EnumClass >
Matrix0D<EnumClass>::Matrix0D() {
    static_assert(std::is_enum<EnumClass>(),"The first template parameter has to be an enum class\n");
}

template<class EnumClass >
EnumClass Matrix0D<EnumClass>::EnumClassIter::operator* () const
{
    assert(pos_ < enum_class_size<EnumClass>());
    return static_cast<EnumClass>(pos_);
}

template<class EnumClass >
const typename Matrix0D<EnumClass>::EnumClassIter& Matrix0D<EnumClass>::EnumClassIter::operator++ ()
{
    ++pos_;
    return *this;
}

template<class EnumClass >
bool Matrix0D<EnumClass>::EnumClassIter::operator!= (const EnumClassIter& other) const
{
    return pos_ != other.pos_;
}

template< class EnumClass >
typename Matrix0D<EnumClass>::EnumClassIter Matrix0D<EnumClass>::begin()
{
    return Matrix0D<EnumClass>::EnumClassIter( this, 0 );
}

template<class EnumClass >
typename Matrix0D<EnumClass>::EnumClassIter Matrix0D<EnumClass>::end()
{
    return Matrix0D<EnumClass>::EnumClassIter( this, enum_class_size<EnumClass>() );
}

template<class Row, class Value, class Accessor>
Value Matrix1D<Row,Value,Accessor>::begin() {
    return data_;
}

template<class Row, class Value, class Accessor>
Value Matrix1D<Row,Value,Accessor>::end() {
    return data_ + enum_class_size<Row>();
}

}; //sfl

#endif //ENUM_CLASS_UTILS_H
