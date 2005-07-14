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

#ifndef PRIVATE_MEMBERS_HPP
#define PRIVATE_MEMBERS_HPP

#include <cassert>

namespace utilspp
{
   namespace private_members
   {
      /**
       * Helper class for utils::set_longevity
       */
      class lifetime_tracker
      {
         public:
            lifetime_tracker( unsigned int longevity );
            virtual ~lifetime_tracker();
            static bool compare( 
                  const lifetime_tracker *l, 
                  const lifetime_tracker *r
                  );

         private:
            unsigned int m_longevity;
      };

      typedef lifetime_tracker** tracker_array;

      extern tracker_array m_tracker_array;
      extern int m_nb_elements;

         /**
          * Helper class for Destroyer
          */
         template< typename T >
         struct deleter
         {
            void delete_object( T *obj );
         };

      /**
       * Concrete lifetime tracker for objects of type T
       */
      template< typename T, typename T_destroyer >
         class concrete_lifetime_tracker : public lifetime_tracker
         {
            public:
               concrete_lifetime_tracker( 
                     T *obj, 
                     unsigned int longevity,
                     T_destroyer d
                     );

               ~concrete_lifetime_tracker();

            private:
               T* m_tracked;
               T_destroyer m_destroyer;
         };

      void at_exit_func();

      template <class T>
         struct adapter
         {
            void operator()(T*);
            void (*m_func)();
         };
   };
};

#include "private_members.inl"

#endif

