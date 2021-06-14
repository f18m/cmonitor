#
# Main makefile for cmonitor project
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)
include $(ROOT_DIR)/Constants.mk

#
# BUILD TARGETS
#

all:
	$(MAKE) -C collector CMONITOR_VERSION=$(CMONITOR_VERSION) CMONITOR_RELEASE=$(CMONITOR_RELEASE) DOCKER_TAG=$(DOCKER_TAG)
	
clean:
	$(MAKE) -C collector clean
	$(MAKE) -C examples clean

install:
ifndef DESTDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the DESTDIR variable. E.g. DESTDIR=/tmp/myinstall"
	@exit 1
endif
ifndef BINDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the BINDIR variable. E.g. BINDIR=usr/bin"
	@exit 1
endif
	$(MAKE) -C collector install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)
	$(MAKE) -C tools/json2html install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)
	$(MAKE) -C tools/statistics install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)

valgrind:
	$(MAKE) -C collector valgrind

examples:
	$(MAKE) -C examples all


.PHONY: all clean install examples 




