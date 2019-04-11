#
# Main makefile for this project
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)
# default rpmbuild source folder
RPM_TMP_DIR:=/tmp/rpm-nmon-cgroup-aware
RPM_VERSION:=22

# targets

all:
	$(MAKE) -C src
	
clean:
	$(MAKE) -C src clean

install:
ifndef DESTDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the DESTDIR variable"
	@exit 1
endif
	$(MAKE) -C src install DESTDIR=$(DESTDIR)
	
generate_patch:
	diff -U3 -w src-orig/njmon_linux_v22.c src/njmon_linux_v22.c > src-orig/cgroup.patch || true


#
# This is useful to produce a binary RPM:
#

rpm:
	mkdir -p $(RPM_TMP_DIR)/
	rm -rf $(RPM_TMP_DIR)/*
	cd $(ROOT_DIR)/.. && \
		tar cvzf $(RPM_TMP_DIR)/nmon-cgroup-aware-$(RPM_VERSION).tar.gz nmon-cgroup-aware/*
	#cd $(RPM_TMP_DIR) && \
	#	tar xvzf nmon-cgroup-aware.tar.gz
	rpmbuild -bb $(ROOT_DIR)/spec/nmon-cgroup-aware.spec \
	  --define "RPM_VERSION $(RPM_VERSION)" \
	  --define "_topdir $(RPM_TMP_DIR)" \
	  --define "_sourcedir $(RPM_TMP_DIR)" \
	  --define "_builddir $(RPM_TMP_DIR)" \
	  --define "_rpmdir $(RPM_TMP_DIR)" && \
		mkdir -p $(outdir)/ && \
		cp -f $(RPM_TMP_DIR)/x86_64/nmon-cgroup-aware-*.rpm $(outdir)/

#
# This target is used by Fedora COPR to automatically produce RPMs for lots of distros.
# COPR will invoke this target like that:
#   make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm.
# E.g.:
#   make -f /home/francesco/work/nmon-cgroup-aware/.copr/Makefile srpm outdir=/tmp/nmon-rpm
srpm:
	mkdir -p $(RPM_TMP_DIR)/
	rm -rf $(RPM_TMP_DIR)/*
	cd $(ROOT_DIR)/.. && \
		tar cvzf $(RPM_TMP_DIR)/nmon-cgroup-aware-$(RPM_VERSION).tar.gz nmon-cgroup-aware/*
	#cd $(RPM_TMP_DIR) && \
	#	tar xvzf nmon-cgroup-aware.tar.gz
	rpmbuild -bs $(ROOT_DIR)/spec/nmon-cgroup-aware.spec \
	  --define "RPM_VERSION $(RPM_VERSION)" \
	  --define "_topdir $(RPM_TMP_DIR)" \
	  --define "_sourcedir $(RPM_TMP_DIR)" \
	  --define "_builddir $(RPM_TMP_DIR)" \
	  --define "_rpmdir $(RPM_TMP_DIR)" && \
		mkdir -p $(outdir)/ && \
		cp -f $(RPM_TMP_DIR)/SRPMS/nmon-cgroup-aware-*.src.rpm $(outdir)/
