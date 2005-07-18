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

#ifndef SINGLETON_HOLDER_HPP
#define SINGLETON_HOLDER_HPP

#include <cassert>

#include "creation_using_new.hpp"
#include "lifetime_default.hpp"
#include "threading_single.hpp"

namespace utilspp
{
   template
   <
   class T,
   template < class > class T_creation_policy = utilspp::creation_using_new,
   template < class > class T_lifetime_policy = utilspp::lifetime_default,
   template < class > class T_threading_model = utilspp::threading_single
   >
   class singleton_holder
   {
      public:
         //the accessor method.
         static T& instance();
         static void make_instance();
         
      protected:
         //protected to be sure that nobody may create one by himself.
         singleton_holder();
         
      private:
         static void destroy_singleton();
         
      private:
         typedef typename T_threading_model< T * >::volatile_type instance_type;
         static instance_type m_instance;
         static bool m_destroyed;
   };
}

#include "singleton_holder.inl"

#endif
