# Copyright (C) 2021 Savoir-faire Linux Inc.

# Author: Olivier Dion <olivier.dion>@savoirfairelinux.com>

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.

QUIET_CC      = @echo    '     CC       ' $@;
QUIET_CXX     = @echo    '     CXX      ' $@;
QUIET_AR      = @echo    '     AR       ' $@;
QUIET_LINK    = @echo    '     LLD      ' $@;
QUIET_LEX     = @echo    '     LEX      ' $@;
QUIET_EXE     = @echo    '     EXE      ' $< $(ARGS);

CPPFLAGS=-I../../src -I. -include ./common.h
CXXFLAGS=-O0 -g -Wall -Wextra
LDFLAGS=-L.

SHARED=libfuzzchannel.so

all: $(SHARED)

.SECONDEXPANSION:
%.so: CXXFLAGS:=$(CXXFLAGS) -fPIC -shared -Wl,-Bsymbolic -Wl,-rpath=$(shell pwd)
%.so: $$(%-objects)
	$(QUIET_LINK) $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $($*-libs)

.SECONDEXPANSION:
%.a: CXXFLAGS:=$(CXXFLAGS) -fPIC -shared -Wl,-Bsymbolic -Wl,-rpath=$(shell pwd)
%.a: $$(%-objects)
	$(QUIET_AR) $(AR) r $@ $^

%.o: %.cpp
	$(QUIET_CXX) $(CXX) $(CPPFLAGS) -MMD $(CXXFLAGS) -c $< -o $@

-include $(shell find . -iname "*.d")

%.cpp: %.ll
	$(QUIET_LEX) $(LEX) --outfile $@ $^

clean:
	find . -iname "*.o" -delete -or -iname "*.so" -delete -or -iname "*.d" -delete -or -iname "*.a" -delete
	rm lib/sip-parser.cpp

print-%:
	@echo $* = $($*)

.PHONY: all clean print

define to-object
$(patsubst %.cpp, %.o, $1)
endef

COMMON=$(call to-object, $(wildcard lib/*.cpp) lib/sip-parser.o)

libfuzzchannel-objects=$(call to-object, fuzz_channel.cpp) $(COMMON)
