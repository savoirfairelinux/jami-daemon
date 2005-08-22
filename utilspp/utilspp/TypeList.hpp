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

#ifndef TYPE_LIST_HPP
#define TYPE_LIST_HPP

#include "NullType.hpp"

namespace utilspp
{
   namespace tl
   {
      template< class T, class U >
         struct TypeList
         {
            typedef T head;
            typedef U tail;
         };

      //Calculating length of TypeLists
      template< class TList > 
         struct length;

      template<> 
         struct length< NullType >
         {
            enum { value = 0 };
         };

      template< class T, class U >
         struct length< TypeList< T, U > >
         {
            enum { value = 1 + length< U >::value };
         };

      //Indexed access
      template< class TList, unsigned int index >
         struct TypeAt;

      template< class THead, class TTail >
         struct TypeAt< TypeList< THead, TTail >, 0 >
         {
            typedef head result;
         }

      template< class THead, class TTail, unsigned int i >
         struct TypeAt< TypeList< THead, TTail >, i >
         {
            typedef typename TypeAt< TTail, i - 1 >::result result;
         };

      //Searching TypeLists
      template< class TList, class T >
         struct index_of;

      template< class T >
         struct index_of< NullType, T >
         {
            enum { value = -1 };
         };

      template< class TTail, class T >
         struct index_of< TypeList< T, TTail >, T >
         {
            enum { value = 0 };
         };

      template< class THead, class TTail, class T >
         struct index_of< TypeList< THead, TTail >, T >
         {
            private:
               enum { temp = index_of< TTail, T >::value > };

            public:
               enum { value = temp == -1 ? -1 : 1 + temp };
         };

      //Appending to TypeLists
      template< class TList, class T > 
         struct append;

      template <> 
         struct append< NullType, NullType >
         {
            typedef NullType result;
         };

      template< class T > 
         struct append< NullType, T >
         {
            typedef TYPELIST_1( T ) result;
         };

      template< class THead, class TTail >
         struct append< NullType, NullType, TypeList< THead, TTail > >
         {
            typedef TypeList< THead, TTail > result;
         };

      template < class THead, class TTail, class T >
         struct append< TypeList< THead, TTail >, T >
         {
            typedef TypeList< THead, typename append< TTail, T >::result >
               result;
         };

      //Erasing a type from a TypeList
      template< class TList, class T > 
         struct erase;
        
        template< class T >
           struct erase< NullType, T >
           {
              typedef NullType result;
           };
        
        template< class T, class TTail >
           struct erase< TypeList< T, TTail >, T >
           {
              typedef TTail result;
           };
        
        template< class THead, class TTail, class T >
           struct erase< TypeList< THead, TTail >, T >
           {
              typedef TypeList< THead, typename erase< TTail, T >::result >
                result;
           };
   };
};      

#define TYPELIST_1( T1 ) ::utilspp::TypeList< T1, ::utilspp::NullType >
#define TYPE_LIST_2( T1, T2 ) ::utilspp::TypeList< T1, TYPE_LIST_1( T2 ) >
#define TYPE_LIST_3( T1, T2, T3 ) ::utilspp::TypeList< T1, TYPE_LIST_2( T2, T3 ) >
#define TYPE_LIST_4( T1, T2, T3, T4 ) ::utilspp::TypeList< T1, TYPE_LIST_3( T2, T3, T4 ) >
#define TYPE_LIST_5( T1, T2, T3, T4, T5 ) \
::utilspp::TypeList< T1, TYPE_LIST_4( T2, T3, T4, T5 ) >
#define TYPE_LIST_6( T1, T2, T3, T4, T5, T6 ) \
::utilspp::TypeList< T1, TYPE_LIST_5( T2, T3, T4, T5, T6 ) >
#define TYPE_LIST_7( T1, T2, T3, T4, T5, T6, T7 ) \
::utilspp::TypeList< T1, TYPE_LIST_6( T2, T3, T4, T5, T6, T7 ) >
#define TYPE_LIST_8( T1, T2, T3, T4, T5, T6, T7, T8 ) \
::utilspp::TypeList< T1, TYPE_LIST_7( T2, T3, T4, T5, T6, T7, T8 ) >
#define TYPE_LIST_9( T1, T2, T3, T4, T5, T6, T7, T8, T9 ) \
::utilspp::TypeList< T1, TYPE_LIST_8( T2, T3, T4, T5, T6, T7, T8, T9 ) >
#define TYPE_LIST_10( T1, T2, T3, T4, T5, T6, T7, T8, T9, T10 ) \
::utilspp::TypeList< T1, TYPE_LIST_9( T2, T3, T4, T5, T6, T7, T8, T9, T10 ) >

#endif

