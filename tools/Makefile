#
# cmonitor tools makefile
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

# constants
THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(shell readlink -f $(THIS_DIR)/..)
include $(ROOT_DIR)/Constants.mk

TOOLS = \
	chart/cmonitor_chart.py \
	filter/cmonitor_filter.py \
	statistics/cmonitor_statistics.py

SYMLINKS = \
	chart/cmonitor_chart \
	filter/cmonitor_filter \
	statistics/cmonitor_statistics

#
# BUILD TARGETS
#

all: $(TOOLS) $(SYMLINKS)

clean:
	rm -f $(TOOLS)

# TOOLS

common-code/cmonitor_version.py: common-code/cmonitor_version.py.j2
	cp common-code/cmonitor_version.py.j2 common-code/cmonitor_version.py
	sed -i 's@__RPM_VERSION__@$(CMONITOR_VERSION)@g' common-code/cmonitor_version.py
	sed -i 's@__RPM_RELEASE__@$(CMONITOR_RELEASE)@g' common-code/cmonitor_version.py

$(TOOLS): common-code/cmonitor_version.py

$(SYMLINKS): common-code/cmonitor_version.py
	@ln -sfv $(shell readlink -f $@.py) $@


#
# RPM TARGETS
#

#
# This target is used by Fedora COPR to automatically produce RPMs for lots of distros.
# COPR will invoke this target like that:
#   make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm.
# E.g.:
#   make -f /home/francesco/work/cmonitor/.copr/Makefile srpm outdir=/tmp/cmonitor-rpm
srpm: $(SYMLINKS)
ifndef outdir
	@echo "*** ERROR: please call this makefile supplying explicitly the outdir variable"
	@exit 1
endif
	# now build the SRPM and copy it to the $(outdir) provided by COPR
	# IMPORTANT: use the spec file that has been edited by the "srpm_tarball" to replace __RPM_VERSION__ 
	cd $(THIS_DIR) && \
		yum install python3-setuptools && \
		python3 setup.py bdist_rpm && \
		cp -fv dist/cmonitor-tools-*.src.rpm $(outdir)/


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






