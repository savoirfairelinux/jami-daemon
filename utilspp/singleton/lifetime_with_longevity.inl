#include <algorithm>
#include <stdexcept>

template< typename T, typename T_destroyer >
void
utilspp::set_longevity( T *obj, unsigned int longevity, T_destroyer d )
{
   using namespace utilspp::private_members;

   tracker_array new_array = static_cast< tracker_array >( 
         std::realloc( 
            m_tracker_array, 
            m_nb_elements + 1
            )
         );
   if( new_array == NULL )
   {
      throw std::bad_alloc();
   }

   lifetime_tracker *p = new concrete_lifetime_tracker< T, T_destroyer >( 
         obj,
         longevity,
         d
         );

   m_tracker_array = new_array;

   tracker_array pos = std::upper_bound( 
         m_tracker_array, 
         m_tracker_array + m_nb_elements,
         p,
         &lifetime_tracker::compare
         );
   std::copy_backward( 
         pos, 
         m_tracker_array + m_nb_elements, 
         m_tracker_array + m_nb_elements + 1
         );

   *pos = p;
   m_nb_elements++;
   std::atexit( &at_exit_func );
};

template< typename T >
void 
utilspp::lifetime_with_longevity< T >::schedule_destruction( T *obj, void (*func)() )
{
   utilspp::private_members::adapter<T> adapter = { func };
   utilspp::set_longevity( obj, get_longevity( obj ), adapter );
}

template< typename T >
void 
utilspp::lifetime_with_longevity< T >::on_dead_reference()
{
   throw std::logic_error("Dead reference detected");
}

template< typename T >
unsigned int 
utilspp::get_longevity( T * )
{
   return 1000;
}


