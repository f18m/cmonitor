#
# Main makefile for this project
#

# constants
# default rpmbuild source folder
RPM_SOURCE_DIR:=~/rpmbuild/SOURCES
RPM_OUTPUT_SRPM_DIR:=~/rpmbuild/SRPMS

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

generate_rpm:
	cd .. && \
		tar cvzf nmon-cgroup-aware.tar.gz nmon-cgroup-aware/* && \
		cp -f nmon-cgroup-aware.tar.gz $(RPM_SOURCE_DIR)/
	cd $(RPM_SOURCE_DIR) && \
		tar xvzf nmon-cgroup-aware.tar.gz
	rpmbuild -bb spec/nmon-cgroup-aware.spec

#
# This target is used by Fedora COPR to automatically produce RPMs for lots of distros.
# COPR will invoke this target like that:
#   make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm
srpm:
	cd .. && \
		tar cvzf nmon-cgroup-aware.tar.gz nmon-cgroup-aware/* && \
		mkdir -p $(RPM_SOURCE_DIR)/ && \
		cp -f nmon-cgroup-aware.tar.gz $(RPM_SOURCE_DIR)/
	cd $(RPM_SOURCE_DIR) && \
		tar xvzf nmon-cgroup-aware.tar.gz
	rpmbuild -bs spec/nmon-cgroup-aware.spec && \
		mkdir -p $(outdir)/ && \
		cp -f $(RPM_OUTPUT_SRPM_DIR)/nmon-cgroup-aware-*.src.rpm $(outdir)/
