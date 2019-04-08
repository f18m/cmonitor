Name:           nmon-cgroup-aware
Version:        22
Release:        1%{?dist}
Summary:        Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness
License:        GPL
URL:            https://github.com/f18m/nmon-cgroup-aware
Source0:        https://github.com/f18m/nmon-cgroup-aware/archive/v22.tar.gz
##BuildRequires:  gcc-c++, libpcap-devel, make

%description
Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness. 
Makes it easy to monitor your LXC/Docker container performances.

%prep
cd %{_topdir}/BUILD
rm -rf nmon-cgroup-aware-%{version}
cp -rf %{_topdir}/SOURCES/nmon-cgroup-aware .

%build
cd nmon-cgroup-aware
%make_build

%install
rm -rf %{buildroot}
cd nmon-cgroup-aware
%make_install

%files
%{_bindir}/njmon
