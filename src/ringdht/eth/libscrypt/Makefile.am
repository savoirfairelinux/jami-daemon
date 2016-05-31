include $(top_srcdir)/globals.mak

if USE_DHT

noinst_LTLIBRARIES = libscrypt.la
libscrypt_la_CFLAGS = @CFLAGS@

libscrypt_la_SOURCES = slowequals.c \
	sha256.c \
	crypto_scrypt-hexconvert.c \
	crypto-mcf.c \
	crypto_scrypt-nosse.c \
	b64.c \
	crypto_scrypt-check.c

endif
