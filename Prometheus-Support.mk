#
# Includes flags and libraries to compile&link with "prometheus-cpp" library
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

ifeq ($(PROMETHEUS_SUPPORT),1)
# include prometheus-cpp depedency through the GNU make variables provided by Conan at install time
# NOTE: for some reason "dl pthread" libraries must be explicitly listed
include $(ROOT_DIR)/conanbuildinfo.mak
LIBS += $(foreach libdir,$(CONAN_LIB_DIRS),-L$(libdir)) $(foreach lib,$(CONAN_LIBS),-l$(lib)) -ldl -lpthread
CXXFLAGS += -DPROMETHEUS_SUPPORT=1 $(CONAN_CXXFLAGS) -I$(CONAN_INCLUDE_DIRS_PROMETHEUS-CPP)
endif