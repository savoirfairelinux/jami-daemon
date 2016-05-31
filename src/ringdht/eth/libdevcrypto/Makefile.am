include $(top_srcdir)/globals.mak

if USE_DHT

AM_CPPFLAGS += -I../
noinst_LTLIBRARIES = libdevcrypto.la
libdevcrypto_la_CXXFLAGS = @CXXFLAGS@

libdevcrypto_la_SOURCES = AES.cpp \
	ECDHE.cpp \
	Common.cpp \
	WordList.cpp \
	CryptoPP.cpp \
	SecretStore.cpp

endif
