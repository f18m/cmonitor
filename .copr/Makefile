THIS_DIR:=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
ROOT_DIR:=$(THIS_DIR)/..
include $(ROOT_DIR)/Constants.mk

srpm:
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	$(MAKE) -C $(ROOT_DIR)/collector srpm
	$(MAKE) -C $(ROOT_DIR)/tools srpm
