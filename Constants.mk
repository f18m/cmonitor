#
# Small makefile snippet meant to be included by other makefiles
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

# default rpmbuild source folder
RPM_TMP_DIR:=/tmp/cmonitor/rpm
RPM_TARBALL_DIR:=/tmp/cmonitor/tarball

# main versioning constants
# IMPORTANT: other places where the version must be updated:
#  - debian/changelog             -> to release a new Ubuntu package
# See also https://github.com/f18m/cmonitor/wiki/new-release
CMONITOR_VERSION:=2.2
CMONITOR_RELEASE:=0

ifeq ($(DOCKER_LATEST),1)
DOCKER_TAG=latest
else
DOCKER_TAG=v$(CMONITOR_VERSION)-$(CMONITOR_RELEASE)
endif

ifeq ($(FMTLIB_MAJOR_VER),)
# assume fmtlib is >= 6.x.y
FMTLIB_MAJOR_VER:=6
endif

$(info CMONITOR_VERSION version is $(CMONITOR_VERSION), CMONITOR_RELEASE release is $(CMONITOR_RELEASE), DOCKER_TAG is $(DOCKER_TAG))
