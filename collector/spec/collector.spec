Name:           cmonitor-collector
Version:        __RPM_VERSION__
Release:        __RPM_RELEASE__
Summary:        A Docker/LXC, database-free, lightweight container performance monitoring solution
License:        GPL
URL:            https://github.com/f18m/cmonitor
Source0:        cmonitor-collector-__RPM_VERSION__.tar.gz

# these are the requirements that we need on COPR builds:
BuildRequires:  gcc-c++, make, gtest-devel
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
%make_build DISABLE_UNIT_TESTS_BUILD=1 DISABLE_BENCHMARKS_BUILD=1

%install
rm -rf %{buildroot}
# we use j2cli during the "install" phase... 
#pip3 install j2cli    # for unknown reasons the pip3 program works but then the 'j2' utility won't work
%make_install -C collector BINDIR=%{_bindir}

%files
%{_bindir}/cmonitor_collector
