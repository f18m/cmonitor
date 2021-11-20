#
# cmonitor_collector makefile
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/..)
include $(ROOT_DIR)/Constants.mk

#
# BUILD TARGETS
#

all:
	$(MAKE) -C src RPM_VERSION=$(CMONITOR_VERSION) RPM_RELEASE=$(CMONITOR_RELEASE) DISABLE_UNIT_TESTS_BUILD=$(DISABLE_UNIT_TESTS_BUILD)

test: 
	$(MAKE) -C src test
	
clean:
	$(MAKE) -C src clean

strip:
	$(MAKE) -C src strip

install:
ifndef DESTDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the DESTDIR variable. E.g. DESTDIR=/tmp/myinstall"
	@exit 1
endif
ifndef BINDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the BINDIR variable. E.g. BINDIR=usr/bin"
	@exit 1
endif
	$(MAKE) -C src install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)

valgrind:
	$(MAKE) -C src valgrind



#
# RPM TARGETS
#

srpm_tarball:
	$(MAKE) clean # make sure we don't include any build result in the source RPM!
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	# prepare the tarball with
	#  a) the whole project inside a versioned folder
	#  b) the spec file with the version inside it replaced
	cp -arf $(ROOT_DIR)/ $(RPM_TARBALL_DIR)/ && \
		cd $(RPM_TARBALL_DIR) && \
		rm -rf cmonitor/examples && \
		mv cmonitor cmonitor-collector-$(CMONITOR_VERSION) && \
		sed -i 's@__RPM_VERSION__@$(CMONITOR_VERSION)@g' cmonitor-collector-$(CMONITOR_VERSION)/collector/spec/collector.spec && \
		sed -i 's@__RPM_RELEASE__@$(CMONITOR_RELEASE)@g' cmonitor-collector-$(CMONITOR_VERSION)/collector/spec/collector.spec && \
		tar cvzf $(RPM_TMP_DIR)/cmonitor-collector-$(CMONITOR_VERSION).tar.gz cmonitor-collector-$(CMONITOR_VERSION)/*
#
# This target is used by Fedora COPR to automatically produce RPMs for lots of distros.
# COPR will invoke this target like that:
#   make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm.
# E.g.:
#   make -f /home/francesco/work/cmonitor/.copr/Makefile srpm outdir=/tmp/cmonitor-rpm
srpm: srpm_tarball
ifndef outdir
	@echo "*** ERROR: please call this makefile supplying explicitly the outdir variable"
	@exit 1
endif
	# now build the SRPM and copy it to the $(outdir) provided by COPR
	# IMPORTANT: use the spec file that has been edited by the "srpm_tarball" to replace __RPM_VERSION__ 
	rpmbuild -bs $(RPM_TARBALL_DIR)/cmonitor-collector-$(CMONITOR_VERSION)/collector/spec/collector.spec \
	  --define "_topdir $(RPM_TMP_DIR)" \
	  --define "_sourcedir $(RPM_TMP_DIR)" \
	  --define "_builddir $(RPM_TMP_DIR)" \
	  --define "_rpmdir $(RPM_TMP_DIR)" && \
		mkdir -p $(outdir)/ && \
		rm -f $(outdir)/cmonitor-collector-*.src.rpm && \
		cp -fv $(RPM_TMP_DIR)/SRPMS/cmonitor-collector-*.src.rpm $(outdir)/

#
# This is useful to produce a binary RPM:
# (This is not used by COPR but still may be useful for local tests)
#
rpm: srpm
ifndef outdir
	@echo "*** ERROR: please call this makefile supplying explicitly the outdir variable"
	@exit 1
endif
	cd $(outdir) && \
		rpmbuild --define "_rpmdir $(RPM_TMP_DIR)" --rebuild cmonitor-collector-*.src.rpm  && \
		mkdir -p $(outdir)/ && \
		cp -fv $(RPM_TMP_DIR)/x86_64/cmonitor-*.rpm $(outdir)/


#
# DOCKER IMAGE
# 

cmonitor_musl: # build cmonitor inside a Docker from Alpine distro, to build a cmonitor_collector musl-linked
	@# use a Docker to compile inside an Alpine environment:
	@# NOTE: by default debug symbols are embededed in the binary (to support COPR builds) but
	#        for docker builds, we strip them away to reduce docker size:
	docker run --rm -it -v "${ROOT_DIR}":"/opt/src" -e "MUSL_BUILD=1" \
		radupopescu/musl-builder \
		sh -c "cd /opt/src/collector && apk add gtest-dev && gcc --version && make clean && make -j && make strip"

docker_image: cmonitor_musl
	@cp -fv bin/musl/cmonitor_collector docker
	docker build \
		--tag f18m/cmonitor:$(DOCKER_TAG) \
		--build-arg sampling_interval=3 \
		--build-arg num_samples=0 \
		docker

docker_run:
	@docker rm cmonitor-baremetal-collector >/dev/null 2>&1 || true
	@docker run -it \
		--rm \
		--name=cmonitor-baremetal-collector \
		-v /root:/perf \
		f18m/cmonitor:$(DOCKER_TAG)

docker_push:
	# this requires write permission in the DockerHub repository:
	# before this target you need to run:
	#    docker login
	@docker push f18m/cmonitor:$(DOCKER_TAG)

.PHONY: all clean install \
		srpm_tarball srpm rpm \
		cmonitor_musl docker_image docker_run




