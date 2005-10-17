#include <qurl.h>

#include "Url.hpp"

static uchar hex_to_int( uchar c )
{
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if ( c >= '0' && c <= '9')
        return c - '0';
    return 0;
}

void Url::decode( QString& url )
{
    int oldlen = url.length();
    if ( !oldlen )
        return;

    int newlen = 0;

    QString newUrl;

    int i = 0;
    while ( i < oldlen ) {
        ushort c = url[ i++ ].unicode();
        if ( c == '%' ) {
            c = hex_to_int( url[ i ].unicode() ) * 16 + hex_to_int( url[ i + 1 ].unicode() );
            i += 2;
        }
	else if ( c == '+' ) {
	  c = ' ';
	}
        newUrl [ newlen++ ] = c;
    }

    url = newUrl;
}
