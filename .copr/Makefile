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
# For issues / questions, see
#     https://pagure.io/copr/copr
#
# This Makefile is currently invoked twice:
# 1) from "collector" subdirectory; test by running
#        export BASE_DIR=/home/fmontorsi/git     (adjust to your case)
#        cd ${BASEDIR}/cmonitor/collector
#        make -f ${BASEDIR}/cmonitor/.copr/Makefile srpm outdir=/tmp/cmonitor-collector-rpm
# 2) from "tools" subdirectory; test by running
#        cd ${BASEDIR}/cmonitor/tools
#        make -f ${BASEDIR}/cmonitor/.copr/Makefile srpm outdir=/tmp/cmonitor-tools-rpm
#
# NOTE: 
# see https://docs.pagure.org/copr.copr/user_documentation.html#how-long-do-you-keep-the-builds
#     https://pagure.io/copr/copr/issue/2050
#
srpm:
	@echo "Env variables in this COPR build:"
	@export
	@echo "Installing git client (required later on to embed commit hash into RPM binaries):"
	dnf -y install git
	git config --global --add safe.directory $(shell readlink -f ..)
	@echo "Running srpm target from .copr/Makefile"
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	$(MAKE) -C $(COPR_CURRENT_SUBDIRECTORY) srpm

rebuild_all_copr_packages:
	# IMPORTANT: take the COPR_TOKEN from the webpage: https://copr.fedorainfracloud.org/coprs/f18m/cmonitor/integrations/
ifndef COPR_TOKEN
	@echo "*** ERROR: please call this makefile supplying explicitly the COPR_TOKEN variable"
	@exit 1
endif
	curl -X POST https://copr.fedorainfracloud.org/webhooks/custom/24475/$(COPR_TOKEN)/cmonitor-collector/
	curl -X POST https://copr.fedorainfracloud.org/webhooks/custom/24475/$(COPR_TOKEN)/cmonitor-tools/
