Name:           cmonitor-tools
Version:        __RPM_VERSION__
Release:        __RPM_RELEASE__
Summary:        A Docker/LXC, database-free, lightweight container performance monitoring solution
License:        GPL
URL:            https://github.com/f18m/cmonitor
Source0:        cmonitor-tools-__RPM_VERSION__.tar.gz
BuildRequires:  gcc-c++, make

# Disable automatic debug package creation: it fails within Fedora 28, 29 and 30 for the lack
# of debug info files apparently:
%global debug_package %{nil}

%description
Tools to post-process data collected via cmonitor_collector utility.
Cmonitor collector is  a Docker/LXC, database-free, lightweight container performance monitoring solution, perfect for ephemeral containers
(e.g. containers used for DevOps automatic testing). Can also be used with InfluxDB and Grafana to monitor long-lived 
containers in real-time.

%prep
%setup

%build

%install
rm -rf %{buildroot}
# we use j2cli during the "install" phase... 
pip install j2cli
%make_install -C tools BINDIR=%{_bindir}

%files
%{_bindir}/cmonitor_chart
%{_bindir}/cmonitor_statistics
