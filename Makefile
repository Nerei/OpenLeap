CL_RED="\033[31m"
CL_GRN="\033[32m"
CL_YLW="\033[33m"
CL_BLU="\033[34m"
CL_MAG="\033[35m"
CL_CYN="\033[36m"
CL_RST="\033[0m"

SHELL = /usr/bin/env bash
UNAME := $(shell uname)

SOURCESRAW=$(filter-out incomplete/%.cpp common/%.cpp classes/%.cpp,$(wildcard */*.cpp) $(wildcard *.cpp))
COMMONRAW=$(wildcard common/*.cpp)
CLASSESRAW=$(wildcard classes/*.cpp)

SOURCES=$(filter-out $(DIAF),$(SOURCESRAW))
COMMON=$(filter-out $(DIAF),$(COMMONRAW))
CLASSES=$(filter-out $(DIAF),$(CLASSESRAW))

define get-target-name
	@echo $(subst $(shell dirname $(1))/,,$(1))
endef

TARGETS = $(basename $(strip $(SOURCES)))

OBJS =  $(subst .cpp,.o,$(COMMON)) \
				$(subst .cpp,.o,$(CLASSES)) \

DIRT = $(wildcard */*.o */*.so */*.d *.i *~ */*~ *.log)

CXXOPTS = -fmessage-length=0 -Wall
ifdef DEBUG	
	CXXOPTS += -DDEBUG -O0 -g
else
	CXXOPTS += -O3
endif

CXXINCS = "-I$(CURDIR)/include" \
          $(shell pkg-config --cflags opencv) \
          $(shell pkg-config --cflags libusb-1.0)

LDLIBS = $(shell pkg-config --libs opencv) \
         $(shell pkg-config --libs libusb-1.0) \
         -lboost_thread-mt

CXXFLAGS = $(CXXOPTS) $(CXXDEFS) $(CXXINCS)
LDFLAGS = $(LDOPTS) $(LDDIRS) $(LDLIBS)

RAWBUS=$(shell lsusb -d f182:0003 | cut -d ' ' -f 2)
RAWDEVICE=$(shell lsusb -d f182:0003 | cut -d ' ' -f 4 | sed 's/:*//g')
BUS=$(shell echo $(RAWBUS) | sed 's/^0*//')
DEVICE=$(shell echo $(RAWDEVICE) | sed 's/^0*//')
file=$(shell mktemp)
tempcap=/tmp/tmp.pcap
TIMETOCAP=5

.PHONY: Makefile

default all: common/leap_libusb_init.c.inc
ifdef DEBUG
	@echo "Building with debugging"
endif
	$(MAKE) $(TARGETS)
	
common/leap_init.pcap:
	@lsmod | grep usbmon || echo Requesting root permissions to modprobe usbmon && sudo modprobe usbmon
	@sudo common/startcapping.sh $(RAWBUS) $(tempcap) $(TIMETOCAP)
	@sudo chown $(shell whoami).$(shell whoami) $(tempcap)
	@mv $(tempcap) common/leap_init.pcap

common/leap_libusb_init.c.inc: common/leap_init.pcap
	common/make_leap_usbinit.sh common/leap_init.pcap common/leap_libusb_init.c.inc $(DEVICE) $(RAWDEVICE)

$(TARGETS): $(OBJS)

%: %.cpp
	@echo -e $(CL_GRN) BIN: $@$(CL_RST)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

%.i: %.cpp
	@echo $@
	$(CXX) -E $(CXXFLAGS) $< | uniq > $@

_clean:
	@$(RM) $(DIRT)

_rmtargets:
	@$(RM) $(TARGETS)

_rmlibusbfiles:
	@$(RM) common/leap_init.pcap common/leap_libusb_init.c.inc

clean: _clean _rmtargets
	@echo "Removed everything except compiled executables."

rmtargets: _rmtargets
	@echo "Removed executables."

clobber: _clean _rmtargets _rmlibusbfiles
	@echo "Removed objects and executables."

.PHONY: fresh
fresh:
	$(MAKE) clean
	$(MAKE) all
