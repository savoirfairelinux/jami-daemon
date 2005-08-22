/*
 *    Copyright (c) <2002-2004> <Jean-Philippe Barrette-LaPierre>
 *    
 *    Permission is hereby granted, free of charge, to any person obtaining
 *    a copy of this software and associated documentation files 
 *    (cURLpp), to deal in the Software without restriction, 
 *    including without limitation the rights to use, copy, modify, merge,
 *    publish, distribute, sublicense, and/or sell copies of the Software,
 *    and to permit persons to whom the Software is furnished to do so, 
 *    subject to the following conditions:
 *    
 *    The above copyright notice and this permission notice shall be included
 *    in all copies or substantial portions of the Software.
 *    
 *    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *    OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 *    IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 *    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 *    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef UTILSPP_TYPETRAIT_HPP
#define UTILSPP_TYPETRAIT_HPP

#include "NullType.hpp"

namespace utilspp
{
  template< typename T >
  class TypeTrait
  {
  private:
    template< typename U >
    struct unreference
    {
      typedef U type;
    };

    template< typename U >
    struct unreference< U & >
    {
      typedef U type;
    };

    template< typename U >
    struct unconst
    {
      typedef U type;
    };

    template< typename U >
    struct unconst< const U >
    {
      typedef U type;
    };

  public:
    typedef typename unreference< T >::type NonReference;
    typedef typename unconst< T >::type NonConst;
    typedef typename unconst< unreference< T >::type >::type NonParam;
  };

  template< class T >
  struct PointerOnMemberFunction
  {
    typedef ::utilspp::NullType ClassType;
    typedef ::utilspp::NullType ReturnType;
    typedef ::utilspp::NullType ParamType;
  };

  template< typename V, typename W, typename R >
  struct PointerOnMemberFunction< W(V::*)(R) >
  {
    typedef V ClassType;
    typedef W ReturnType;
    typedef R ParamType;
  };

  template< typename T >
  struct PointerOnFunction
  {
    typedef utilspp::NullType ReturnType;
    typedef utilspp::NullType Param1Type;
    typedef utilspp::NullType Param2Type;
    typedef utilspp::NullType Param3;
    typedef utilspp::NullType Param4Type;
    typedef utilspp::NullType Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V >
  struct PointerOnFunction< V(*)() >
  {
    typedef V ReturnType;
    typedef utilspp::NullType Param1Type;
    typedef utilspp::NullType Param2Type;
    typedef utilspp::NullType Param3Type;
    typedef utilspp::NullType Param4Type;
    typedef utilspp::NullType Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W >
  struct PointerOnFunction< V(*)(W) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef utilspp::NullType Param2Type;
    typedef utilspp::NullType Param3Type;
    typedef utilspp::NullType Param4Type;
    typedef utilspp::NullType Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W, typename X >
  struct PointerOnFunction< V(*)(W, X) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef X Param2Type;
    typedef utilspp::NullType Param3Type;
    typedef utilspp::NullType Param4Type;
    typedef utilspp::NullType Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W, typename X, typename Y >
  struct PointerOnFunction< V(*)(W, X, Y) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef X Param2Type;
    typedef Y Param3Type;
    typedef utilspp::NullType Param4Type;
    typedef utilspp::NullType Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W, typename X, typename Y, typename Z >
  struct PointerOnFunction< V(*)(W, X, Y, Z) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef X Param2Type;
    typedef Y Param3Type;
    typedef Z Param4Type;
    typedef utilspp::NullType Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W, typename X, typename Y, typename Z, typename A >
  struct PointerOnFunction< V(*)(W, X, Y, Z, A) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef X Param2Type;
    typedef Y Param3Type;
    typedef Z Param4Type;
    typedef A Param5Type;
    typedef utilspp::NullType Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W, typename X, typename Y, typename Z, typename A, typename B >
  struct PointerOnFunction< V(*)(W, X, Y, Z, A, B) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef X Param2Type;
    typedef Y Param3Type;
    typedef Z Param4Type;
    typedef A Param5Type;
    typedef B Param6Type;
    typedef utilspp::NullType Param7Type;
  };

  template< typename V, typename W, typename X, typename Y, typename Z, typename A, typename B, typename C >
  struct PointerOnFunction< V(*)(W, X, Y, Z, A, B, C) >
  {
    typedef V ReturnType;
    typedef W Param1Type;
    typedef X Param2Type;
    typedef Y Param3Type;
    typedef Z Param4Type;
    typedef A Param5Type;
    typedef B Param6Type;
    typedef C Param7Type;
  };

};

#endif
