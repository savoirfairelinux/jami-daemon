SUBDIRS = singleton

noinst_LTLIBRARIES = libutilspp.la

libutilspp_la_SOURCES = \
	EmptyType.hpp \
	NonCopyable.hpp \
	NullType.hpp \
	Singleton.hpp \
	ThreadingFactoryMutex.hpp ThreadingFactoryMutex.inl \
	ThreadingSingle.hpp ThreadingSingle.inl

libutilspp_la_LIBADD = ./singleton/libsingleton.la
