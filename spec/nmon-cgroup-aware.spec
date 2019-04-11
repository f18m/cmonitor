Name:           nmon-cgroup-aware
Version:        __RPM_VERSION__
Release:        1%{?dist}
Summary:        Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness
License:        GPL
URL:            https://github.com/f18m/nmon-cgroup-aware
Source0:        nmon-cgroup-aware-__RPM_VERSION__.tar.gz
##Source0:        https://github.com/f18m/nmon-cgroup-aware/archive/v22.tar.gz
##BuildRequires:  gcc-c++, libpcap-devel, make

%description
Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness. 
Makes it easy to monitor your LXC/Docker container performances.

%prep
# expanded version of "setup" macro:
%setup
#cd %{_topdir}
#rm -rf nmon-cgroup-aware #-%{version}
#tar xvf nmon-cgroup-aware-%RPM_VERSION.tar.gz
#cp -rf %{_topdir}/SOURCES/nmon-cgroup-aware .

%build
%make_build

%install
rm -rf %{buildroot}
%make_install

%files
%{_bindir}/njmon
