Name:           nmon-cgroup-aware
Version:        __RPM_VERSION__
Release:        __RPM_RELEASE__
Summary:        Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness
License:        GPL
URL:            https://github.com/f18m/nmon-cgroup-aware
Source0:        nmon-cgroup-aware-__RPM_VERSION__.tar.gz
#BuildRequires:  gcc-c++, make

%description
Fork of Nigel's performance Monitor for Linux, adding cgroup-awareness. 
Makes it easy to monitor your LXC/Docker container performances.

%prep
%setup

%build
%make_build

%install
rm -rf %{buildroot}
%make_install BINDIR=%{_bindir}

%files
%{_bindir}/njmon_collector
%{_bindir}/njmon_chart
