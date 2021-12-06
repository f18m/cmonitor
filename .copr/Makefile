# this points to either "collector" or "tools" subdirectory, see below
COPR_CURRENT_SUBDIRECTORY:=$(shell readlink -f .)
ROOT_DIR:=$(COPR_CURRENT_SUBDIRECTORY)/..
include $(ROOT_DIR)/Constants.mk

$(info Launching COPR build from directory: $(COPR_CURRENT_SUBDIRECTORY))

#
# This target is used by Fedora COPR to automatically produce RPMs for lots of distros.
# COPR will invoke this target like that:
#   make -f <cloned_repodir>/.copr/Makefile srpm outdir="<outdir>" spec="<spec_path>"
# See https://docs.pagure.org/copr.copr/user_documentation.html#make-srpm.
#
# This Makefile is currently invoked twice:
# 1) from "collector" subdirectory; test by running
#        cd /home/fmontorsi/git/cmonitor/collector
#        make -f /home/fmontorsi/git/cmonitor/.copr/Makefile srpm outdir=/tmp/cmonitor-collector-rpm
# 2) from "tools" subdirectory; test by running
#        cd /home/fmontorsi/git/cmonitor/tools
#        make -f /home/fmontorsi/git/cmonitor/.copr/Makefile srpm outdir=/tmp/cmonitor-tools-rpm
srpm:
	@echo "Env variables in this COPR build:"
	export
	@echo "Running srpm target from .copr/Makefile"
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	$(MAKE) -C $(COPR_CURRENT_SUBDIRECTORY) srpm
