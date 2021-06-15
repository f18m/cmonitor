#
# cmonitor tools makefile
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
	
clean:

install:
	@mkdir -p $(DESTDIR)/$(BINDIR)/
	@cp -fv chart/cmonitor_chart.py             $(DESTDIR)/$(BINDIR)/cmonitor_chart
	@cp -fv statistics/cmonitor_statistics.py   $(DESTDIR)/$(BINDIR)/cmonitor_statistics
	chmod a+x $(DESTDIR)/$(BINDIR)/cmonitor_chart
	chmod a+x $(DESTDIR)/$(BINDIR)/cmonitor_statistics


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
		rm -rf cmonitor/collector && \
		mv cmonitor cmonitor-tools-$(CMONITOR_VERSION) && \
		sed -i 's@__RPM_VERSION__@$(CMONITOR_VERSION)@g' cmonitor-tools-$(CMONITOR_VERSION)/tools/spec/tools.spec && \
		sed -i 's@__RPM_RELEASE__@$(CMONITOR_RELEASE)@g' cmonitor-tools-$(CMONITOR_VERSION)/tools/spec/tools.spec && \
		tar cvzf $(RPM_TMP_DIR)/cmonitor-tools-$(CMONITOR_VERSION).tar.gz cmonitor-tools-$(CMONITOR_VERSION)/*
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
	rpmbuild -bs $(RPM_TARBALL_DIR)/cmonitor-tools-$(CMONITOR_VERSION)/tools/spec/tools.spec \
	  --define "_topdir $(RPM_TMP_DIR)" \
	  --define "_sourcedir $(RPM_TMP_DIR)" \
	  --define "_builddir $(RPM_TMP_DIR)" \
	  --define "_rpmdir $(RPM_TMP_DIR)" && \
		mkdir -p $(outdir)/ && \
		rm -f $(outdir)/cmonitor-tools-*.src.rpm && \
		cp -fv $(RPM_TMP_DIR)/SRPMS/cmonitor-tools-*.src.rpm $(outdir)/

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
		rpmbuild --define "_rpmdir $(RPM_TMP_DIR)" --rebuild cmonitor-tools-*.src.rpm  && \
		mkdir -p $(outdir)/ && \
		cp -fv $(RPM_TMP_DIR)/x86_64/cmonitor-*.rpm $(outdir)/






