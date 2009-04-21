
ifneq (,1)
DIRS += gsm
endif

ifneq (,1)
DIRS += ilbc
endif

ifneq (,1)
DIRS += speex
endif

ifneq ($(findstring pa,pa_unix),)
DIRS += portaudio
endif
