Name:           cmonitor-collector
Version:        __RPM_VERSION__
Release:        __RPM_RELEASE__%{?dist}
Summary:        A Docker/LXC, database-free, lightweight container performance monitoring solution
License:        GPL
URL:            https://github.com/f18m/cmonitor
Source0:        cmonitor-collector-__RPM_VERSION__.tar.gz
Requires:       fmt

# these are the requirements that we need on COPR builds:
# gcc-c++, make    basic compiler and GNU make
# git              used by cmonitor Makefiles to get latest commit hash and include it in --version output
# gtest-devel      for gtest-based unit tests
# fmt-devel        libfmt dependency; note however cmonitor-collector RPM is built also on the 'old' 
#                  Centos7 platform shipping fmt-devel-6.2.1 so make sure not to use any feature of libfmt > 6.2.1
# zlib-devel,       
#    cmake3,         
#    python3-pip, 
#    python3-setuptools, 
#    perl  
#                  requirements for libprometheus and its build system (Conan-based, cmake3-based);
#                  note that for some reason from FC35 and up we also need to request 'setuptools' pypi to install
#                  successfully the 'conan' pypi, and we install it with python3-setuptools
#                  perl is instead required from FC35 upward to build OpenSSL Conan package successfully
BuildRequires:  gcc-c++, make, git, gtest-devel, fmt-devel, zlib-devel, cmake3, python3-pip, python3-setuptools, perl

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
# first of all install in the buildroot the Conan package manager, which we use to fetch
# prometheus-cpp, since that library is not, unfortunately, packaged by Fedora/COPR
# NOTE: prometheus-cpp & its dependencies wants at least Conan 1.51.0
echo "[Inside RPM build] installing Conan"
pip3 install --user 'conan==1.51.2' 
echo "[Inside RPM build] bootstrapping Conan"
conan profile new %{buildroot}/cmonitor_rpmbuild --detect 
conan profile update settings.compiler.libcxx=libstdc++11 %{buildroot}/cmonitor_rpmbuild
conan remote list

# secondly, Conan is used to fetch prometheus-cpp library, building it with cmake when needed:
# NOTE: civetweb dependency has broken Conan package asking for 'cmake' instead of 'cmake3'
#       so we need to create a "cmake" binary in %{buildroot}/bin which points to 'cmake3'
echo "[Inside RPM build] installing prometheus-cpp"
mkdir -p %{buildroot}/bin
ln -sf /usr/bin/cmake3 %{buildroot}/bin/cmake
export PATH="%{buildroot}/bin:$PATH"
echo "[Inside RPM build] the PATH adjusted to contain a cmake3->cmake symlink is: $PATH"
conan install conanfile.txt --build=missing --profile %{buildroot}/cmonitor_rpmbuild

# finally, this command invokes the root Makefile of cmonitor repo, from inside the source tarball
# produced by COPR; that root Makefile will pass all the options listed here to collector/Makefile
echo "[Inside RPM build] launching cmonitor collector build"
%make_build PROMETHEUS_SUPPORT=1 DISABLE_UNIT_TESTS_BUILD=1 DISABLE_BENCHMARKS_BUILD=1 FMTLIB_MAJOR_VER=6 CMONITOR_LAST_COMMIT_HASH=__LAST_COMMIT_HASH__

%install
rm -rf %{buildroot}
# we use j2cli during the "install" phase... 
#pip3 install j2cli    # for unknown reasons the pip3 program works but then the 'j2' utility won't work
%make_install -C collector BINDIR=%{_bindir}

%files
%{_bindir}/cmonitor_collector
