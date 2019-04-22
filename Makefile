#
# Main makefile for this project
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)

# default rpmbuild source folder
RPM_TMP_DIR:=/tmp/nmon-cgroup-aware/rpm
RPM_TARBALL_DIR:=/tmp/nmon-cgroup-aware/tarball

# reason for this version is that this is a fork of Nigel's performance Monitor v22
RPM_VERSION:=22

# RPM_RELEASE is what is actually incremented release after release!
RPM_RELEASE:=2


#
# BUILD TARGETS
#

all:
	$(MAKE) -C src
	
clean:
	$(MAKE) -C src clean

install:
ifndef DESTDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the DESTDIR variable"
	@exit 1
endif
ifndef BINDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the BINDIR variable"
	@exit 1
endif
	$(MAKE) -C src install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)
	$(MAKE) -C njmonchart install DESTDIR=$(DESTDIR) BINDIR=$(BINDIR)



#
# AUXILIARY TARGETS
#

generate_patch:
	diff -U3 -w src-orig/njmon_linux_v22.c src/njmon_linux_v22.c > src-orig/cgroup.patch || true

examples:
	$(MAKE) -C examples all



#
# RPM TARGETS
#

srpm_tarball:
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	# prepare the tarball with
	#  a) the whole project inside a versioned folder
	#  b) the spec file with the version inside it replaced
	cp -arf $(THIS_DIR) $(RPM_TARBALL_DIR)/ && \
		cd $(RPM_TARBALL_DIR) && \
		mv nmon-cgroup-aware nmon-cgroup-aware-$(RPM_VERSION) && \
		sed -i 's@__RPM_VERSION__@$(RPM_VERSION)@g' nmon-cgroup-aware-$(RPM_VERSION)/spec/nmon-cgroup-aware.spec && \
		sed -i 's@__RPM_RELEASE__@$(RPM_RELEASE)@g' nmon-cgroup-aware-$(RPM_VERSION)/spec/nmon-cgroup-aware.spec && \
		tar cvzf $(RPM_TMP_DIR)/nmon-cgroup-aware-$(RPM_VERSION).tar.gz nmon-cgroup-aware-$(RPM_VERSION)/*
#
# This target is used by Fedora COPR to automatically produce RPMs for lots of distros.
# COPR will invoke this target like that:
#   make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm.
# E.g.:
#   make -f /home/francesco/work/nmon-cgroup-aware/.copr/Makefile srpm outdir=/tmp/nmon-rpm
srpm: srpm_tarball
ifndef outdir
	@echo "*** ERROR: please call this makefile supplying explicitly the outdir variable"
	@exit 1
endif
	# now build the SRPM and copy it to the $(outdir) provided by COPR
	# IMPORTANT: use the spec file that has been edited by the "srpm_tarball" to replace __RPM_VERSION__ 
	rpmbuild -bs $(RPM_TARBALL_DIR)/nmon-cgroup-aware-$(RPM_VERSION)/spec/nmon-cgroup-aware.spec \
	  --define "_topdir $(RPM_TMP_DIR)" \
	  --define "_sourcedir $(RPM_TMP_DIR)" \
	  --define "_builddir $(RPM_TMP_DIR)" \
	  --define "_rpmdir $(RPM_TMP_DIR)" && \
		mkdir -p $(outdir)/ && \
		rm -f $(outdir)/nmon-cgroup-aware-*.src.rpm && \
		cp -fv $(RPM_TMP_DIR)/SRPMS/nmon-cgroup-aware-*.src.rpm $(outdir)/

#
# This is useful to produce a binary RPM:
#
rpm: srpm
ifndef outdir
	@echo "*** ERROR: please call this makefile supplying explicitly the outdir variable"
	@exit 1
endif
	cd $(outdir) && \
		rpmbuild --define "_rpmdir $(RPM_TMP_DIR)" --rebuild nmon-cgroup-aware-*.src.rpm  && \
		mkdir -p $(outdir)/ && \
		cp -fv $(RPM_TMP_DIR)/x86_64/nmon-cgroup-aware-*.rpm $(outdir)/

.PHONY: all clean install examples generate_patch srpm_tarball srpm rpm
