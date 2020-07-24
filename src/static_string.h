// Copyright (C) 2017 - 2018 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#pragma once

#include <type_traits>
#include <cassert>
#include <cstddef>

#define AK_TOOLKIT_CONFIG_USING_STRING_VIEW 1

// # Based on value of macro AK_TOOLKIT_CONFIG_USING_STRING_VIEW we decide if and how
//   we want to handle a conversion to string_view

# if defined AK_TOOLKIT_CONFIG_USING_STRING_VIEW
#   if AK_TOOLKIT_CONFIG_USING_STRING_VIEW == 0
#     define AK_TOOLKIT_STRING_VIEW_OPERATIONS()
#   elif AK_TOOLKIT_CONFIG_USING_STRING_VIEW == 1
#     include <string_view>
#     define AK_TOOLKIT_STRING_VIEW_OPERATIONS() constexpr operator ::std::string_view () const { return ::std::string_view(c_str(), size()); }
#   elif AK_TOOLKIT_CONFIG_USING_STRING_VIEW == 2
#     include <experimental/string_view>
#     define AK_TOOLKIT_STRING_VIEW_OPERATIONS() constexpr operator ::std::experimental::string_view () const { return ::std::experimental::string_view(c_str(), size()); }
#   elif AK_TOOLKIT_CONFIG_USING_STRING_VIEW == 3
#     include <boost/utility/string_ref.hpp> 
#     define AK_TOOLKIT_STRING_VIEW_OPERATIONS() constexpr operator ::boost::string_ref () const { return ::boost::string_ref(c_str(), size()); }
#   elif AK_TOOLKIT_CONFIG_USING_STRING_VIEW == 4
#     include <string> 
#     define AK_TOOLKIT_STRING_VIEW_OPERATIONS() operator ::std::string () const { return ::std::string(c_str(), size()); }
#   endif
# else
#   define AK_TOOLKIT_STRING_VIEW_OPERATIONS()
# endif

#if __cplusplus >= 201402L

#include <utility>
namespace ak_toolkit { namespace static_str { namespace detail {

template <int... Is>
  using int_sequence = std::integer_sequence<int, Is...>;

template <int N>
  using make_int_sequence = std::make_integer_sequence<int, N>;

}}}

#else // not C++14
	
namespace ak_toolkit { namespace static_str { namespace detail {
 
// # Implementation of a subset of C++14 std::integer_sequence and std::make_integer_sequence
 
  template <int... I>
  struct int_sequence
  {};
 
  template <int i, typename T>
  struct cat
  {
    static_assert (sizeof(T) < 0, "bad use of cat");
  };
 
  template <int i, int... I>
  struct cat<i, int_sequence<I...>>
  {
    using type = int_sequence<I..., i>;
  };
 
  template <int I>
  struct make_int_sequence_
  {
    static_assert (I >= 0, "bad use of make_int_sequence: negative size");
    using type = typename cat<I - 1, typename make_int_sequence_<I - 1>::type>::type;
  };
 
  template <>
  struct make_int_sequence_<0>
  {
    using type = int_sequence<>;
  };
 
  template <int I>
  using make_int_sequence = typename make_int_sequence_<I>::type;

}}} // namespace ak_toolkit::static_str::detail

#endif // not C++14



namespace ak_toolkit { namespace static_str {

// # size_tag and segment_tag are used to enable constant int inference in constructor templates
template <int N> struct size_tag {};
template <int N,  int M> struct segment_tag {};

// # Implementation of a constexpr-compatible assertion

#if defined NDEBUG
# define AK_TOOLKIT_ASSERT(CHECK) void(0)
#else
# define AK_TOOLKIT_ASSERT(CHECK) ((CHECK) ? void(0) : []{assert(!#CHECK);}())
#endif


struct literal_ref {};
struct char_array {};
struct literal_suffix {};
struct array_suffix {};

template <typename Tag>
struct array_upgrade
{
  typedef Tag type;
};

template <>
struct array_upgrade<literal_ref>
{
  typedef char_array type;
};

template <>
struct array_upgrade<literal_suffix>
{
  typedef array_suffix type;
};

template <typename Tag1, typename tag2>
struct select_conctenated_array_tag
{
  typedef array_suffix type;
};

template <>
struct select_conctenated_array_tag<char_array, char_array>
{
  typedef char_array type;
};

template <typename Tag1, typename Tag2>
struct concat_tag
{
  typedef typename select_conctenated_array_tag<typename array_upgrade<Tag1>::type,
                                                typename array_upgrade<Tag2>::type>::type type;
};



template <int N, typename Impl = literal_ref>
class string
{
	static_assert (N > 0 && N < 0, "Invalid specialization of string");
};

// # A wraper over a string literal with alternate interface. No ownership management

template <int N>
class string<N, literal_ref>
{
	static_assert (N >= 0, "string with negative length would be created");
	
    const char (&_lit)[N + 1];
    
    template <int, typename> friend class string;
    
public:
    constexpr string(const char (&lit)[N + 1]) : _lit((AK_TOOLKIT_ASSERT(lit[N] == 0), lit)) {}
    constexpr char operator[](int i) const { return AK_TOOLKIT_ASSERT(i >= 0 && i < N), _lit[i]; }
    AK_TOOLKIT_STRING_VIEW_OPERATIONS()
    constexpr ::std::size_t size() const { return N; };
    constexpr const char* c_str() const { return _lit; }
    constexpr operator const char * () const { return c_str(); }
};

template <int N>
  using string_literal = string<N, literal_ref>;


// # A function that converts raw string literal into string_literal and deduces the size.

template <int N_PLUS_1>
constexpr string_literal<N_PLUS_1 - 1> literal(const char (&lit)[N_PLUS_1])
{
    return string_literal<N_PLUS_1 - 1>(lit);
}


// # This implements a null-terminated array that stores elements on stack.

template <int N>
class string<N, char_array>
{
	static_assert (N >= 0, "string with negative length would be created");
	
    char _array[N + 1];
    struct private_ctor {};
    
    template <int M, typename TL, typename TR, int... Il, int... Ir>
    constexpr explicit string(private_ctor, string<M, TL> const& l, string<N - M, TR> const& r, detail::int_sequence<Il...>, detail::int_sequence<Ir...>)
      : _array{l[Il]..., r[Ir]..., 0}
    {
    }
   
    template <typename T, int... Il>
    constexpr explicit string(private_ctor, T const& l,
                              detail::int_sequence<Il...>,
                              int offset
                              )
      : _array{l[Il + offset]..., 0}
    {
    }
   
public:
    template <int M, typename TL, typename TR, typename std::enable_if<(M <= N), bool>::type = true>
    constexpr explicit string(string<M, TL> l, string<N - M, TR> r, int, int)
    : string(private_ctor{}, l, r, detail::make_int_sequence<M>{}, detail::make_int_sequence<N - M>{})
    {
    }
    
    template <int N2_plus_1, int from>
    constexpr explicit string(const char (&lit)[N2_plus_1], size_tag<from>)
    : string(private_ctor{}, lit, detail::make_int_sequence<N2_plus_1 - 1 - from>{}, from)
    {
    }

    template <int M, typename T,  int from,  int len>
    constexpr explicit string(string<M, T> const& str, segment_tag<from, len>)
    : string(private_ctor{}, str, detail::make_int_sequence<len>{}, from)
    {
    }
    
    constexpr string(string_literal<N> l) // converting
    : string(private_ctor{}, l, detail::make_int_sequence<N>{}, 0)
    {
    }

    template < int N2_plus_1,  int from,  int len>
    constexpr explicit string(const char (&lit)[N2_plus_1], segment_tag<from, len>)
    : string(private_ctor{}, lit, detail::make_int_sequence<len>{}, from)
    {
    }
   
    constexpr ::std::size_t size() const { return N; }
  
    constexpr const char* c_str() const { return _array; }
    constexpr operator const char * () const { return c_str(); }
    AK_TOOLKIT_STRING_VIEW_OPERATIONS()
    constexpr char operator[] (int i) const { return AK_TOOLKIT_ASSERT(i >= 0 && i < N), _array[i]; }
};

template <int N>
  using array_string = string<N, char_array>;


// # This implements a suffix of a null-terminated array that stores elements on stack.

template <int N>
class string<N, array_suffix>
{
	static_assert (N >= 0, "string with negative length would be created");
	
    char _array[N + 1];
    int _offset;
    // invariant: 0 <= _offset && _offset <= N
    
    struct private_ctor {};
    
    template <int M, typename TL, typename TR>
    constexpr static char permute1(string<M, TL> const& l, string<N - M, TR> const& r, int lsize, int rsize, int i)
    {
      return i < M - lsize + N - M - rsize ? '\0' : l[i - (M - lsize + N - M - rsize)];
    }
    
    template <int M, typename TL, typename TR>
    constexpr static char permute2(string<M, TL> const& l, string<N - M, TR> const& r, int lsize, int rsize, int i)
    {
      return i < N - M - rsize ? l[lsize - (N - M - rsize) + i] : r[i - (N - M - rsize)];
    }
    
    template <int M, int... Il, int... Ir, typename TL, typename TR>
    constexpr explicit string(private_ctor, string<M, TL> const& l, string<N - M, TR> const& r, detail::int_sequence<Il...>, detail::int_sequence<Ir...>,
                              int lsize, int rsize)
      : _array{permute1(l, r, lsize, rsize, Il)..., permute2(l, r, lsize, rsize, Ir)..., 0}
      , _offset(N - lsize - rsize)
    {
    }
   
    template <int... Il, typename T>
    constexpr explicit string(private_ctor, T const& l,
                              detail::int_sequence<Il...>,
                              int offset
                              )
      : _array{l[Il + offset]..., 0}
      , _offset(offset)
    {
    }
   
public:
    template <int M, typename TL, typename TR, typename std::enable_if<(M <= N), bool>::type = true>
    constexpr explicit string(string<M, TL> l, string<N - M, TR> r, int lsize, int rsize)
    : string(private_ctor{}, l, r, detail::make_int_sequence<M>{}, detail::make_int_sequence<N - M>{}, lsize, rsize)
    {
    }
    
    template <int N2_plus_1, int from>
    constexpr explicit string(const char (&lit)[N2_plus_1], size_tag<from>)
    : string(private_ctor{}, lit, detail::make_int_sequence<N2_plus_1 - 1 - from>{}, from)
    {
    }
    
    constexpr string(string_literal<N> l) // converting
    : string(private_ctor{}, l, detail::make_int_sequence<N>{}, 0)
    {
    }
   
    constexpr ::std::size_t size() const { return N - _offset; }
  
    constexpr const char* c_str() const { return _array + _offset; }
    constexpr operator const char * () const { return c_str(); }
    AK_TOOLKIT_STRING_VIEW_OPERATIONS()
    constexpr char operator[] (int i) const { return AK_TOOLKIT_ASSERT(i >= 0 && i < size()), _array[i + _offset]; }
};

template <int N>
  using array_string_suffix = string<N, array_suffix>;
  
// # An implementation with compile-time capacity and constexpr length

template <int N>
class string<N, literal_suffix>
{
	static_assert (N >= 0, "string with negative length would be created");
	
    const char (&_lit)[N + 1];
    int _offset;
    
    // invariant: _offset >= 0 && _offset <= N
    
public:
    constexpr string(const char (&lit)[N + 1], int offset)
      : _lit((AK_TOOLKIT_ASSERT(lit[N] == 0), lit))
      , _offset((AK_TOOLKIT_ASSERT(0 <= offset && offset <= N), offset)) {}
      
    constexpr string(string_literal<N> l, int offset)
      : _lit(l._lit)
      , _offset((AK_TOOLKIT_ASSERT(0 <= offset && offset <= N), offset)) {}
      
    constexpr string(const char (&lit)[N + 1]) : _lit((AK_TOOLKIT_ASSERT(lit[N] == 0), lit)), _offset(0) {}
    constexpr char operator[](int i) const { return AK_TOOLKIT_ASSERT(i >= 0 && i < size()), _lit[i + _offset]; }
    AK_TOOLKIT_STRING_VIEW_OPERATIONS()
    constexpr ::std::size_t size() const { return N - _offset; }
    constexpr const char* c_str() const { return _lit + _offset; }
    constexpr operator const char * () const { return c_str(); }
};

template <int N>
  using string_literal_suffix = string<N, literal_suffix>;


template <int N>
constexpr string_literal_suffix<N> suffix(string_literal<N> l, int offset)
{
  return AK_TOOLKIT_ASSERT(0 <= offset && offset <= N), string_literal_suffix<N>(l, offset);
}

// # A function that converts raw string literal + offset into string_literal and deduces the size.

template <int OFFSET, int N_PLUS_1, typename std::enable_if<(OFFSET >= 0 && OFFSET < N_PLUS_1), bool>::type = true>
constexpr array_string<N_PLUS_1 - 1 - OFFSET> offset_literal(const char (&lit)[N_PLUS_1])
{
    return AK_TOOLKIT_ASSERT(lit[N_PLUS_1 - 1] == 0), array_string<N_PLUS_1 - 1 - OFFSET>(lit, size_tag<OFFSET>{});
}

template <int OFFSET, int N_PLUS_1, typename std::enable_if<!(OFFSET >= 0 && OFFSET < N_PLUS_1), bool>::type = true>
void offset_literal(const char (&lit)[N_PLUS_1])
{
    static_assert(OFFSET >= 0 && OFFSET < N_PLUS_1, "bad offset provided to offset_literal");
}

template <int OFFSET, int LEN , int N_PLUS_1,
        typename std::enable_if<(OFFSET >= 0 && OFFSET < N_PLUS_1), bool>::type = true,
        typename std::enable_if<(LEN >= 0 && OFFSET + LEN < N_PLUS_1), bool>::type = true>
constexpr array_string<LEN> substr(const char (&lit)[N_PLUS_1])
{
    return AK_TOOLKIT_ASSERT(lit[N_PLUS_1 - 1] == '\0'), array_string<LEN>(lit, segment_tag<OFFSET, LEN>{});
}

template <int OFFSET, int LEN , int N, typename T,
      typename std::enable_if<(OFFSET >= 0 && OFFSET <= N), bool>::type = true,
      typename std::enable_if<(LEN >= 0 && OFFSET + LEN <= N), bool>::type = true>
constexpr array_string<LEN> substr(string<N, T> const& sstring)
{
    return array_string<LEN>(sstring, segment_tag<OFFSET, LEN>{});
}

// # A set of concatenating operators, for different combinations of raw literals, string_literal<>, and array_string<>

template <int N1, int N2, typename TL, typename TR>
constexpr string<N1 + N2, typename concat_tag<TL, TR>::type> operator+(string<N1, TL> const& l, string<N2, TR> const& r)
{
    return string<N1 + N2, typename concat_tag<TL, TR>::type>(l, r, l.size(), r.size());
}

template <int N1_1, int N2, typename TR>
constexpr string<N1_1 - 1 + N2, typename array_upgrade<TR>::type> operator+(const char (&l)[N1_1], string<N2, TR> const& r)
{
    return string<N1_1 - 1 + N2, typename array_upgrade<TR>::type>(string_literal<N1_1 - 1>(l), r, N1_1 - 1, r.size());
}

template <int N1, int N2_1, typename TL>
constexpr string<N1 + N2_1 - 1, typename array_upgrade<TL>::type> operator+(string<N1, TL> const& l, const char (&r)[N2_1])
{
    return string<N1 + N2_1 - 1, typename array_upgrade<TL>::type>(l, string_literal<N2_1 - 1>(r), l.size(), N2_1 - 1);
}

template <int N2, typename TR>
::std::string operator+(::std::string const& l, string<N2, TR> const& r)
{
    ::std::string ret;
    ret.reserve(l.size() + N2);
    ret.append(l);
    ret.append(r.c_str(), r.size());
    return ret;
}

template <int N1, typename TL>
::std::string operator+(string<N1, TL> const& l, ::std::string const& r)
{
    ::std::string ret;
    ret.reserve(N1 + r.size());
    ret.append(l.c_str(), l.size());
    ret.append(r);
    return ret;
}

}} // namespace ak_toolkit::static_str
