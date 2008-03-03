SUBDIRS = 

noinst_LTLIBRARIES = libmixer.la

libmixer_la_SOURCES =  	 AudioInput.cpp \
			 AudioMixer2Channels.cpp \
			 AudioMixer.cpp \
			 AudioOuput.cpp \
			 AudioSynch.cpp \
			 InputStreams.cpp \
			 InternalBuffer.cpp \
			 LocalAudioOuput.cpp \
			 LocalVideoOuput.cpp \
			 NoSynch.cpp \
			 StraightThrough_AT.cpp \
			 StraightThrough_VT.cpp \
			 VideoInput.cpp \
			 VideoMixer2Channels.cpp \
			 VideoMixer.cpp \
			 VideoOutput.cpp \
			 TimeInfo.cpp \
			 Mixer.cpp



#AM_CXXFLAGS = -I$(top_srcdir)/include $(USER_INCLUDES)
AM_CXXFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs $(libccext2_CFLAGS) $(libdbuscpp_CFLAGS) $(libccrtp1_CFLAGS) $(USER_INCLUDES)

noinst_HEADERS = AudioInput.h \
		 AudioMixer2Channels.h \
		 AudioMixer.h \
		 AudioOuput.h \
		 AudioSynch.h \
		 InputStreams.h \
		 InternalBuffer.h \
		 LocalAudioOuput.h \
		 LocalVideoOuput.h \
		 Mixer.h \
		 NoSynch.h \
		 OutputStream.h \
		 StraightThrough_AT.h \
		 StraightThrough_VT.h \
		 SynchManager.h \
		 TimeInfo.h \
		 VideoInput.h \
		 VideoMixer2Channels.h \
		 VideoMixer.h \
		 VideoOutput.h

