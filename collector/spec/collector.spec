Name:           cmonitor-collector
Version:        __RPM_VERSION__
Release:        __RPM_RELEASE__
Summary:        A Docker/LXC, database-free, lightweight container performance monitoring solution
License:        GPL
URL:            https://github.com/f18m/cmonitor
Source0:        cmonitor-collector-__RPM_VERSION__.tar.gz
Requires:       fmt

# these are the requirements that we need on COPR builds:
# IMPORTANT: cmonitor-collector RPM is built also on the 'old' Centos7 platform shipping fmt-devel-6.2.1
#            so make sure not to use any feature of libfmt > 6.2.1
BuildRequires:  gcc-c++, make, gtest-devel, fmt-devel
# python3-pip works and gets installed but then it fails later for unknown reasons

# Disable automatic debug package creation: it fails within Fedora 28, 29 and 30 for the lack
# of debug info files apparently:
%global debug_package %{nil}

%description
A Docker/LXC/Kubernetes, database-free, lightweight container performance monitoring solution, 
perfect for ephemeral containers (e.g. containers used for DevOps automatic testing). 
Can also be used with InfluxDB and Grafana.

%prep
%setup

%build
# this command invokes the root Makefile of cmonitor repo, from inside the source tarball
# produced by COPR that will pass all the options listed here to collector/Makefile
%make_build DISABLE_UNIT_TESTS_BUILD=1 DISABLE_BENCHMARKS_BUILD=1 FMTLIB_MAJOR_VER=6 CMONITOR_LAST_COMMIT_HASH=__LAST_COMMIT_HASH__

%install
rm -rf %{buildroot}
# we use j2cli during the "install" phase... 
#pip3 install j2cli    # for unknown reasons the pip3 program works but then the 'j2' utility won't work
%make_install -C collector BINDIR=%{_bindir}

%files
%{_bindir}/cmonitor_collector
