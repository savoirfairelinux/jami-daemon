SHARED=

# Add fuzzing shared lib here
SHARED += fuzz-channel.so

################################################################################
#                            DO NOT EDIT BELOW                                 #
################################################################################
QUIET_AR      = @echo    '     AR       ' $@;
QUIET_CXX     = @echo    '     CXX      ' $@;
QUIET_LINK    = @echo    '     LINK     ' $@;
QUIET_YACC    = @echo    '     YACC     ' $@;
QUIET_LEX     = @echo    '     LEX      ' $@;

CPPFLAGS=-I../../src -I../../contrib/x86_64-pc-linux-gnu/include -I. -include ./common.h
CXXFLAGS=-O0 -ggdb3 -Wall -Wextra

ARCHIVE=

ARCHIVE += libfuzz.a
libfuzz-objects = $(patsubst %.cpp,%.o,$(wildcard lib/*.cpp) lib/sip-parser.cpp)

.SECONDEXPANSION:
%.so: CXXFLAGS:=$(CXXFLAGS) -fPIC -shared -Wl,-Bsymbolic
%.so: %.o
	$(QUIET_LINK) $(CXX) $(CPPFLAGS) $(CXXFLAGS) -Wl,--whole-archive libfuzz.a -Wl,--no-whole-archive -o $@ $^ $($*-libs)

.SECONDEXPANSION:
%.a: CXXFLAGS:=$(CXXFLAGS) -fPIC -Wl,-Bsymbolic
%.a: $$(%-objects)
	$(QUIET_AR) ar rcs $@ $^

%.o: %.cpp
	$(QUIET_CXX) $(CXX) $(CPPFLAGS) -MMD $(CXXFLAGS) -c $< -o $@

%.cpp: %.ll
	$(QUIET_LEX) lex -o $@ $^

-include $($(shell find . -iname "*.o"):%.o=%.d)

all: $(ARCHIVE) $(SHARED)

print-%:
	@echo $* = $($*)

clean:
	find . -iname "*.o" -delete -or -iname "*.d" -delete -or -iname "*.so" -delete -or -iname "*.a" -delete

.PHONY: all clean print
.PRECIOUS: %.o
