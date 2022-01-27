#
# FIXME: we should probably rewrite this spec file looking at https://docs.fedoraproject.org/en-US/packaging-guidelines/Python/
#

Name:           cmonitor-tools
Version:        __RPM_VERSION__
Release:        __RPM_RELEASE__
Summary:        A Docker/LXC, database-free, lightweight container performance monitoring solution
License:        GPL
URL:            https://github.com/f18m/cmonitor
Source0:        cmonitor-tools-__RPM_VERSION__.tar.gz

# these are the requirements that we need on COPR builds:
# IMPORTANT: python3-devel provide macros like %{python3_sitelib}
BuildRequires:  gcc-c++, make, python3-devel

# cmonitor_filter uses dateutil library to parse dates:
Requires: python-dateutil

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
%make_install -C tools BINDIR=%{_bindir} PYTHON3_SITELIB=%{python3_sitelib}

%files
%{_bindir}/cmonitor_*
%{python3_sitelib}/cmonitor_*.py
%{python3_sitelib}//__pycache__/cmonitor_*.*.pyc
