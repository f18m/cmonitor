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
CMONITOR_VERSION:=2.6.0
CMONITOR_RELEASE:=0

ifeq ($(CMONITOR_LAST_COMMIT_HASH),)
# when building RPMs we need to provide this value from inside the .spec file,
# so it gets passed to GNU make externally. In all other cases, we can ask git:
CMONITOR_LAST_COMMIT_HASH:=$(shell git rev-parse HEAD)
endif

ifeq ($(DOCKER_LATEST),1)
DOCKER_TAG=latest
else
DOCKER_TAG=v$(CMONITOR_VERSION)-$(CMONITOR_RELEASE)
endif

ifeq ($(FMTLIB_MAJOR_VER),)
# assume fmtlib is >= 6.x.y
FMTLIB_MAJOR_VER:=6
endif

$(info INFO: CMonitor project version is $(CMONITOR_VERSION)-$(CMONITOR_RELEASE), last commit hash is $(CMONITOR_LAST_COMMIT_HASH), DOCKER_TAG is $(DOCKER_TAG))

# useful defines for gcc:
DEFS = -DVERSION_STRING=\"$(CMONITOR_VERSION)-$(CMONITOR_RELEASE)\"
DEFS += -DCMONITOR_LAST_COMMIT_HASH=\"$(CMONITOR_LAST_COMMIT_HASH)\"
DEFS += -DFMTLIB_MAJOR_VER=$(FMTLIB_MAJOR_VER)
