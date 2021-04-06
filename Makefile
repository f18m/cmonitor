#
# Main makefile for cmonitor project
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)

# default rpmbuild source folder
RPM_TMP_DIR:=/tmp/cmonitor/rpm
RPM_TARBALL_DIR:=/tmp/cmonitor/tarball

# main versioning constants
# IMPORTANT: other places where the version must be updated:
#  - debian/changelog             -> to release a new Ubuntu package
#  - json2html/cmonitor_chart.py  -> look for CMONITOR_VERSION
# See also https://github.com/f18m/cmonitor/wiki/new-release
CMONITOR_VERSION:=1.4
CMONITOR_RELEASE:=4

ifeq ($(DOCKER_LATEST),1)
DOCKER_TAG=latest
else
DOCKER_TAG=v$(CMONITOR_VERSION)-$(CMONITOR_RELEASE)
endif


$(info RPM version is $(CMONITOR_VERSION), RPM release is $(CMONITOR_RELEASE))

#
# BUILD TARGETS
#

all:
	$(MAKE) -C src RPM_VERSION=$(CMONITOR_VERSION) RPM_RELEASE=$(CMONITOR_RELEASE)
	
clean:
	$(MAKE) -C src clean
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
	$(MAKE) -C src install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)
	$(MAKE) -C json2html install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)
	$(MAKE) -C statistics install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)

valgrind:
	$(MAKE) -C src valgrind

examples:
	$(MAKE) -C examples all



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
	cp -arf $(THIS_DIR) $(RPM_TARBALL_DIR)/ && \
		cd $(RPM_TARBALL_DIR) && \
		mv cmonitor cmonitor-$(CMONITOR_VERSION) && \
		sed -i 's@__RPM_VERSION__@$(CMONITOR_VERSION)@g' cmonitor-$(CMONITOR_VERSION)/spec/cmonitor.spec && \
		sed -i 's@__RPM_RELEASE__@$(CMONITOR_RELEASE)@g' cmonitor-$(CMONITOR_VERSION)/spec/cmonitor.spec && \
		tar cvzf $(RPM_TMP_DIR)/cmonitor-$(CMONITOR_VERSION).tar.gz cmonitor-$(CMONITOR_VERSION)/*
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
	rpmbuild -bs $(RPM_TARBALL_DIR)/cmonitor-$(CMONITOR_VERSION)/spec/cmonitor.spec \
	  --define "_topdir $(RPM_TMP_DIR)" \
	  --define "_sourcedir $(RPM_TMP_DIR)" \
	  --define "_builddir $(RPM_TMP_DIR)" \
	  --define "_rpmdir $(RPM_TMP_DIR)" && \
		mkdir -p $(outdir)/ && \
		rm -f $(outdir)/cmonitor-*.src.rpm && \
		cp -fv $(RPM_TMP_DIR)/SRPMS/cmonitor-*.src.rpm $(outdir)/

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
		rpmbuild --define "_rpmdir $(RPM_TMP_DIR)" --rebuild cmonitor-*.src.rpm  && \
		mkdir -p $(outdir)/ && \
		cp -fv $(RPM_TMP_DIR)/x86_64/cmonitor-*.rpm $(outdir)/



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
deb:
ifeq ($(shell whoami),root)
	@echo "*** ERROR: generating the Debian/Ubuntu package requires using a normal user, having a valid GPG secret key registered;"
	@echo "           both 'gpg -k' and 'gpg -K' must show the same key."
	@exit 1
endif
	dpkg-buildpackage -tc -S --force-sign # build source only otherwise Ubuntu PPA rejects with "Source/binary (i.e. mixed) uploads are not allowed"
	## PROBABLY NOT NECESSARY: debsign -S            # you must have the GPG key setup properly for this to work (see e.g. https://help.github.com/en/articles/generating-a-new-gpg-key)
	@echo "When ready to upload to your PPA run dput as e.g.:"
	@echo "    cd .. && dput ppa:francesco-montorsi/cmonitor cmonitor_$(CMONITOR_VERSION).$(CMONITOR_RELEASE)-1ubuntu1_source.changes"

#
# DOCKER IMAGE
# 

cmonitor_musl: # build cmonitor inside a Docker from Alpine distro, to build a cmonitor_collector musl-linked
	# use a Docker to compile inside an Alpine environment:
	docker run --rm -it -v "${PWD}":"/opt/src" radupopescu/musl-builder \
		sh -c "cd /opt/src && gcc --version && make clean && make"

docker_image: cmonitor_musl
	@cp -fv src/cmonitor_collector docker
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

.PHONY: all clean install examples \
		srpm_tarball srpm rpm \
		cmonitor_musl docker_image docker_run




