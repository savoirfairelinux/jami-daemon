template< typename T >
void
utilspp::private_members::deleter< T >::delete_object( T *obj )
{
   delete obj;
}

template< typename T, typename T_destroyer >
utilspp::private_members::concrete_lifetime_tracker< T, T_destroyer >::concrete_lifetime_tracker( 
      T *obj,
      unsigned int longevity,
      T_destroyer d
      ) 
: lifetime_tracker( longevity )
, m_tracked( obj )
, m_destroyer( d )
{}

template< typename T, typename T_destroyer >
utilspp::private_members::concrete_lifetime_tracker< T, T_destroyer >::~concrete_lifetime_tracker()
{
   m_destroyer( m_tracked );
}


template < typename T >
void
utilspp::private_members::adapter< T >::operator()(T*) 
{ 
   return (*m_func)(); 
}
