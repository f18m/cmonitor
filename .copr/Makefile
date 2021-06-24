THIS_DIR:=$(shell readlink -f .)
ROOT_DIR:=$(THIS_DIR)/..
include $(ROOT_DIR)/Constants.mk

$(info Launching COPR build from directory: $(THIS_DIR))

srpm:
	mkdir -p $(RPM_TMP_DIR)/ $(RPM_TARBALL_DIR)/
	rm -rf $(RPM_TMP_DIR)/* $(RPM_TARBALL_DIR)/*
	$(MAKE) -C $(THIS_DIR) srpm
