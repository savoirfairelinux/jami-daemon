#include <speex/speex.h>
#include <cstdio>
#include <cmath>

int main()
{
    const SpeexMode *mode = speex_lib_get_mode(SPEEX_MODEID_NB);
    printf("Hello %s %f\n", mode->modeName, M_PI);
    return 0;
}
