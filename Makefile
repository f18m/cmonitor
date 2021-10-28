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
	# reason why we have so many "if directory exists" is that during COPR builds, 
	# the source RPM tarball will contain only a subset of the folders
	if [ -d "collector" ]; then	$(MAKE) -C collector CMONITOR_VERSION=$(CMONITOR_VERSION) CMONITOR_RELEASE=$(CMONITOR_RELEASE) DOCKER_TAG=$(DOCKER_TAG) ; fi
	if [ -d "tools" ]; then	$(MAKE) -C tools CMONITOR_VERSION=$(CMONITOR_VERSION) CMONITOR_RELEASE=$(CMONITOR_RELEASE) ; fi
	if [ -d "examples" ]; then	$(MAKE) -C examples CMONITOR_VERSION=$(CMONITOR_VERSION) CMONITOR_RELEASE=$(CMONITOR_RELEASE) ; fi
	
clean:
	$(MAKE) -C collector clean
	$(MAKE) -C tools clean
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
	$(MAKE) -C tools/chart install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)
	$(MAKE) -C tools/statistics install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)

valgrind:
	$(MAKE) -C collector valgrind
cmonitor_musl:
	$(MAKE) -C collector cmonitor_musl
docker_image:
	$(MAKE) -C collector docker_image
docker_run:
	$(MAKE) -C collector docker_run
docker_push:
	$(MAKE) -C collector docker_push

examples:
	$(MAKE) -C examples all

.PHONY: all clean install examples 




