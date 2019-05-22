#
# Main makefile for this project
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)

# default rpmbuild source folder
RPM_TMP_DIR:=/tmp/cmonitor/rpm
RPM_TARBALL_DIR:=/tmp/cmonitor/tarball

# reason for this version is that this is a fork of Nigel's performance Monitor v22
RPM_VERSION:=22

# RPM_RELEASE is what is actually incremented release after release!
RPM_RELEASE:=7


#
# BUILD TARGETS
#

all:
	$(MAKE) -C src RPM_VERSION=$(RPM_VERSION) RPM_RELEASE=$(RPM_RELEASE)
	
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

valgrind:
	$(MAKE) -C src valgrind

examples:
	$(MAKE) -C examples all



#
# RPM TARGETS
#

srpm_tarball:
	$(MAKE) -C src RPM_VERSION=$(RPM_VERSION) RPM_RELEASE=$(RPM_RELEASE)
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	# prepare the tarball with
	#  a) the whole project inside a versioned folder
	#  b) the spec file with the version inside it replaced
	cp -arf $(THIS_DIR) $(RPM_TARBALL_DIR)/ && \
		cd $(RPM_TARBALL_DIR) && \
		mv cmonitor cmonitor-$(RPM_VERSION) && \
		sed -i 's@__RPM_VERSION__@$(RPM_VERSION)@g' cmonitor-$(RPM_VERSION)/spec/cmonitor.spec && \
		sed -i 's@__RPM_RELEASE__@$(RPM_RELEASE)@g' cmonitor-$(RPM_VERSION)/spec/cmonitor.spec && \
		tar cvzf $(RPM_TMP_DIR)/cmonitor-$(RPM_VERSION).tar.gz cmonitor-$(RPM_VERSION)/*
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
	rpmbuild -bs $(RPM_TARBALL_DIR)/cmonitor-$(RPM_VERSION)/spec/cmonitor.spec \
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
	dpkg-buildpackage -S  # build source only otherwise Ubuntu PPA rejects with "Source/binary (i.e. mixed) uploads are not allowed"
	## PROBABLY NOT NECESSARY: debsign -S            # you must have the GPG key setup properly for this to work (see e.g. https://help.github.com/en/articles/generating-a-new-gpg-key)
	@echo "When ready to upload to your PPA run dput as e.g.:"
	@echo "    dput ppa:francesco-montorsi/ppa ../cmonitor_$(RPM_VERSION).$(RPM_RELEASE)-1ubuntu1_source.changes"

#
# DOCKER IMAGE
# 

docker_image:
	@cp -fv src/cmonitor_collector docker
	@docker build \
		--tag f18m/cmonitor:v$(RPM_VERSION)-$(RPM_RELEASE) \
		--build-arg rpm_version=$(RPM_VERSION)-$(RPM_RELEASE) \
		docker

docker_run:
	@docker rm cmonitor-baremetal-collector || true
	@docker run -it \
		--rm \
		--name=cmonitor-baremetal-collector \
		-v /root:/perf \
		f18m/cmonitor:v$(RPM_VERSION)-$(RPM_RELEASE)
	


.PHONY: all clean install examples \
		srpm_tarball srpm rpm \
		docker_image docker_run




