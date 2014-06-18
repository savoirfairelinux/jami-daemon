include $(top_srcdir)/globals.mak

noinst_LTLIBRARIES = libclient_android.la

libclient_android_la_SOURCES = callmanager_jni.cpp          \
                               configurationmanager_jni.cpp

if SFL_PRESENCE
libclient_android_la_SOURCES += presencemanager_jni.cpp
endif

noinst_HEADERS = jni_callbacks.h
