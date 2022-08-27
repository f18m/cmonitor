#
# Includes flags and libraries to compile&link with "prometheus-cpp" library
# https://github.com/f18m/cmonitor
# Francesco Montorsi (c) 2019
#

ifeq ($(PROMETHEUS_SUPPORT),1)

$(info INFO: Prometheus support is ENABLED)

ifeq ($(wildcard $(ROOT_DIR)/conanbuildinfo.mak),)
$(error Please run 'conan install . --build=missing' first)
endif

# include prometheus-cpp depedency through the GNU make variables provided by Conan at install time
# NOTE: for some reason "dl pthread" libraries must be explicitly listed
include $(ROOT_DIR)/conanbuildinfo.mak
LIBS += $(foreach libdir,$(CONAN_LIB_DIRS),-L$(libdir)) $(foreach lib,$(CONAN_LIBS),-l$(lib)) -ldl -lpthread
CXXFLAGS += $(CONAN_CXXFLAGS) -I$(CONAN_INCLUDE_DIRS_PROMETHEUS-CPP)
DEFS += -DPROMETHEUS_SUPPORT=1
else

$(info INFO: Prometheus support is DISABLED)
DEFS += -DNO_PROMETHEUS_SUPPORT

endif
