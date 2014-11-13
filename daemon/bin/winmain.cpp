#include <speex/speex.h>

int main()
{
    const SpeexMode *mode = speex_lib_get_mode(SPEEX_MODEID_NB);
    printf("Hello %s\n", mode->modeName);
    return 0;
}
