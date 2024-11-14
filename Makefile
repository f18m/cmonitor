#
# Main makefile for cmonitor project
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019-2024
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)
include $(ROOT_DIR)/Constants.mk

#
# BUILD TARGETS
#

all:
	@# reason why we have so many "if directory exists" is that during COPR builds, 
	@# the source RPM tarball will contain only a subset of the folders
	if [ -d "collector" ]; then	$(MAKE) -C collector CMONITOR_VERSION=$(CMONITOR_VERSION) CMONITOR_RELEASE=$(CMONITOR_RELEASE) CMONITOR_LAST_COMMIT_HASH=$(CMONITOR_LAST_COMMIT_HASH) DOCKER_TAG=$(DOCKER_TAG) PROMETHEUS_SUPPORT=$(PROMETHEUS_SUPPORT) ; fi
	if [ -d "tools" ]; then	$(MAKE) -C tools CMONITOR_VERSION=$(CMONITOR_VERSION) CMONITOR_RELEASE=$(CMONITOR_RELEASE) CMONITOR_LAST_COMMIT_HASH=$(CMONITOR_LAST_COMMIT_HASH) ; fi

centos_install_prereq:
	# this is just the list present in "BuildRequires" field of the RPM spec file:
	yum install gcc-c++ make gtest-devel fmt-devel git

test:
	$(MAKE) -C collector test
	$(MAKE) -C examples all
	if [ -d "tools" ]; then	$(MAKE) -C tools test ; fi

format_check:
	black --check .

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
	$(MAKE) -C tools install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)

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



# DEBIAN PACKAGE
#
# Reference guide:
#   https://www.debian.org/doc/manuals/maint-guide/dreq.en.html
#
# Quick summary:
#  - update debian/changelog using interactive utility "dch -i"
#    (you must have formatted the debian/changelog file according to Debian/Ubuntu strict rules!!)
#  - make deb
#  - run dput to upload to your PPA
#
# Note: debian control files must live in the root folder of the project, not inside "collector" or "tools"
deb_help:
	@echo " make deb_new_changelog"
	@echo " make deb_local_test"
	@echo " make deb_source_pkg_for_upload"

deb_new_changelog:
	dch -i --distribution bionic --controlmaint

deb_source_pkg_for_upload:
ifeq ($(shell whoami),root)
	@echo "*** ERROR: generating the Debian/Ubuntu package requires using a normal user, having a valid GPG secret key registered;"
	@echo "           both 'gpg -k' and 'gpg -K' must show the same key."
	@exit 1
endif
	dpkg-buildpackage --post-clean --build=source --force-sign  # build source only otherwise Ubuntu PPA rejects with "Source/binary (i.e. mixed) uploads are not allowed"
	debsign -S                                                  # you must have the GPG key setup properly for this to work (see e.g. https://help.github.com/en/articles/generating-a-new-gpg-key)
	@echo "When ready to upload to your PPA run dput as e.g.:"
	@echo "    cd .. && dput ppa:francesco-montorsi/cmonitor cmonitor_$(CMONITOR_VERSION).$(CMONITOR_RELEASE)-1ubuntu1_source.changes"

deb_local_test:
ifeq ($(shell whoami),root)
	@echo "*** ERROR: generating the Debian/Ubuntu package requires using a normal user, having a valid GPG secret key registered;"
	@echo "           both 'gpg -k' and 'gpg -K' must show the same key."
	@exit 1
endif
	dpkg-buildpackage
 
 
.PHONY: all clean install examples 




