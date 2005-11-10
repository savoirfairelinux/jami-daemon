
#if defined(WIN32)

#if defined(__cplusplus)
extern "C" {
#endif

#ifndef EAFNOSUPPORT
#define EAFNOSUPPORT	WSAEAFNOSUPPORT
#endif


const char *_inet_ntop(int af, const void *src, char *dst, size_t size);

#define inet_ntop _inet_ntop

#if defined(__cplusplus)
}
#endif

#endif /* WIN32 */
