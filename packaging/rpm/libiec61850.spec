Name: libiec61850
Version: 1.0.1
Vendor: MZ Automation GmbH
Packager: Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
Release: 1%{?dist}
Summary: IEC 61850 MMS/GOOSE client and server library

License: GPLv3
URL:     http://libiec61850.com/libiec61850/
Source0: libiec61850_1.0.1.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc cmake

%description

libiec61850 is an open-source (GPLv3) implementation of an IEC 61850 client and server library implementing the protocols MMS, GOOSE and SV. It is implemented in C (according to the C99 standard) to provide maximum portability. It can be used to implement IEC 61850 compliant client and server applications on embedded systems and PCs running Linux, Windows, and MacOS. Included is a set of simple example applications that can be used as a starting point to implement own IEC 61850 compliant devices or to communicate with IEC 61850 devices. The library has been successfully used in many commercial software products and devices.

%package devel

Summary:        Headers and libraries for building apps that use libiec61850
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel

The development headers for libiec61850.

%prep
%setup -q -n libiec61850_1.0.1/

%build
mkdir -p build
cd build
%cmake -DBUILD_EXAMPLES=0 -DBUILD_PYTHON_BINDINGS=0 ..
make

%install
rm -rf $RPM_BUILD_ROOT
cd build
make install DESTDIR=$RPM_BUILD_ROOT

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
/usr/lib/libiec61850.*

%files devel
/usr/include/libiec61850/*

%changelog
* Fri Mar 17 2017 Steffen Vogel <stvogel@eonerc.rwth-aachen.de
- Initial RPM release