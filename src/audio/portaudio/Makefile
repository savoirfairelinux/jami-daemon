# Generated automatically from Makefile.in by configure.
#
# PortAudio V19 Makefile.in
#
# Dominic Mazzoni
#

PREFIX = /opt
CC = gcc
CFLAGS = -g -O2 -Wall -DPA_LITTLE_ENDIAN -Ipa_common  -DSIZEOF_SHORT=2 -DSIZEOF_INT=4 -DSIZEOF_LONG=4 -DHAVE_LIBPTHREAD=1 -DPA_USE_ALSA=1 -DPA_USE_OSS=1 
LIBS = -lpthread  -lasound -lm -lpthread
AR = /usr/bin/ar
RANLIB = ranlib
INSTALL = /usr/bin/install -c
SHARED_FLAGS = -shared
DLL_LIBS =  -lasound
CXXFLAGS = 
NASM = 
NASMOPT = 

OTHER_OBJS =  pa_linux_alsa/pa_linux_alsa.o pa_unix_oss/pa_unix_oss.o pa_unix/pa_unix_hostapis.o pa_unix/pa_unix_util.o

PALIB = libportaudio.a
PADLL = libportaudio.so
PADLLV = $(PADLL).0.0.19
PAINC = pa_common/portaudio.h

COMMON_OBJS = \
	pa_common/pa_allocation.o \
	pa_common/pa_converters.o \
	pa_common/pa_cpuload.o \
	pa_common/pa_dither.o \
	pa_common/pa_front.o \
	pa_common/pa_process.o \
	pa_common/pa_skeleton.o \
	pa_common/pa_stream.o \
	pa_common/pa_trace.o

TESTS = \
	bin/paqa_devs \
	bin/paqa_errs \
	bin/patest1 \
	bin/patest_buffer \
	bin/patest_callbackstop \
	bin/patest_clip \
	bin/patest_dither \
	bin/patest_hang \
	bin/patest_in_overflow \
	bin/patest_latency \
	bin/patest_leftright \
	bin/patest_longsine \
	bin/patest_many \
	bin/patest_maxsines \
	bin/patest_multi_sine \
	bin/patest_out_underflow \
	bin/patest_pink \
	bin/patest_prime \
	bin/patest_read_record \
	bin/patest_record \
	bin/patest_ringmix \
	bin/patest_saw \
	bin/patest_sine8 \
	bin/patest_sine \
	bin/patest_sine_formats \
	bin/patest_sine_time \
	bin/patest_start_stop \
	bin/patest_stop \
	bin/patest_sync \
	bin/patest_toomanysines \
	bin/patest_underflow \
	bin/patest_wire \
	bin/patest_write_sine \
	bin/pa_devs \
	bin/pa_fuzz \
	bin/pa_minlat

# Most of these don't compile yet.  Put them in TESTS, above, if
# you want to try to compile them...
ALL_TESTS = \
	bin/debug_convert \
	bin/debug_dither_calc \
	bin/debug_dual \
	bin/debug_multi_in \
	bin/debug_multi_out \
	bin/debug_record \
	bin/debug_record_reuse \
	bin/debug_sine_amp \
	bin/debug_sine \
	bin/debug_sine_formats \
	bin/debug_srate \
	bin/debug_test1 \
	bin/pa_devs \
	bin/pa_fuzz \
	bin/pa_minlat \
	bin/paqa_devs \
	bin/paqa_errs \
	bin/patest1 \
	bin/patest_buffer \
	bin/patest_clip \
	bin/patest_dither \
	bin/patest_hang \
	bin/patest_in_overflow \
	bin/patest_latency \
	bin/patest_leftright \
	bin/patest_longsine \
	bin/patest_many \
	bin/patest_maxsines \
	bin/patest_multi_sine \
	bin/patest_out_underflow \
	bin/patest_pink \
	bin/patest_read_record \
	bin/patest_record \
	bin/patest_ringmix \
	bin/patest_saw \
	bin/patest_sine8 \
	bin/patest_sine \
	bin/patest_sine_formats \
	bin/patest_sine_time \
	bin/patest_start_stop \
	bin/patest_stop \
	bin/patest_sync \
	bin/patest_toomanysines \
	bin/patest_underflow \
	bin/patest_wire \
	bin/patest_write_sine

OBJS = $(COMMON_OBJS) $(OTHER_OBJS)

all: lib/$(PALIB) lib/$(PADLLV) tests

tests: bin/ $(TESTS)

lib/$(PALIB): lib/ $(OBJS) Makefile $(PAINC)
	$(AR) ruv lib/$(PALIB) $(OBJS)
	$(RANLIB) lib/$(PALIB)

lib/$(PADLLV): lib/ $(OBJS) Makefile $(PAINC)
	$(CC) $(SHARED_FLAGS) -o lib/$(PADLLV) $(OBJS) $(DLL_LIBS)

$(TESTS): bin/%: lib/$(PALIB) Makefile $(PAINC) pa_tests/%.c
	$(CC) -o $@ $(CFLAGS) pa_tests/$*.c lib/$(PALIB) $(LIBS)

install: lib/$(PALIB) lib/$(PADLLV)
	$(INSTALL) -m 644 lib/$(PADLLV) $(PREFIX)/lib/$(PADLLV)
	$(INSTALL) -m 644 lib/$(PALIB) $(PREFIX)/lib/$(PALIB)
	cd $(PREFIX)/lib && rm -f $(PADLL) && ln -s $(PADLLV) $(PADLL)
	$(INSTALL) -m 644 pa_common/portaudio.h $(PREFIX)/include/portaudio.h
	@echo ""
	@echo "------------------------------------------------------------"
	@echo "PortAudio was successfully installed."
	@echo ""
	@echo "On some systems (e.g. Linux) you should run 'ldconfig' now"
	@echo "to make the shared object available.  You may also need to"
	@echo "modify your LD_LIBRARY_PATH environment variable to include"
	@echo "the directory $(PREFIX)/lib"
	@echo "------------------------------------------------------------"
	@echo ""

clean:
	rm -f $(OBJS) $(TESTS) lib/$(PALIB) lib/$(PADLLV)

%.o: %.c Makefile $(PAINC)
	$(CC) -c $(CFLAGS) $< -o $@

%.o: %.cpp Makefile $(PAINC)
	$(CXX) -c $(CXXFLAGS) $< -o $@

%.o: %.asm
	$(NASM) $(NASMOPT) -o $@ $<

bin:
	mkdir bin

lib:
	mkdir lib



