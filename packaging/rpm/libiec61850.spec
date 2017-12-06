Name: libiec61850
Version: 1.1
Vendor: MZ Automation
Packager: Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
Release: 1%{?dist}
Summary: Open source libraries for IEC 61850 and IEC 60870-5-104

License: GPL-3.0
URL: https://github.com/mz-automation/libiec61850
#Source0: https://github.com/mz-automation/libiec61850/repository/archive.tar.gz?ref=v%{version}
Source0: libiec61850_1.1.0.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: gcc cmake

%description

IEC 61850 is an international standard for communication systems in Substation Automation Systems (SAS) and management of Decentralized Energy Resources (DER).
It is seen as one of the communication standards of the emerging Smart Grid.
Despite of its complexity and other drawbacks that make it difficult to implement and to use, the protocol is the only one of its kind that is in widespread use by utility companies and equipment manufacturers.

The project libIEC61850 provides a server and client library for the IEC 61850/MMS, IEC 61850/GOOSE and IEC 61850-9-2/Sampled Values communication protocols written in C.
It is available under the GPLv3 license. With this library available everybody can now become easily familiar with IEC 61850.
A tabular overview of the provided IEC 61850 ACSI services is available on the project website.

The library is designed according to edition 2 of the IEC 61850 standard series, but should be compatible to edition 1 in most cases.

%package devel

Summary:        Headers and libraries for building apps that use libiec61850
Group:          Development/Libraries
Requires:       %{name} = %{version}-%{release}

%description devel

The development headers for libiec61850.

%prep
%setup -q -n libiec61850_1.1.0/

%build
mkdir -p build
cd build
%cmake -DCMAKE_INSTALL_LIBDIR=${_libdir} ..
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
cd build
make install DESTDIR=$RPM_BUILD_ROOT

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig

%clean
rm -rf $RPM_BUILD_ROOT

%files
%{_prefix}/lib/libiec61850.so.1.1.0
%{_prefix}/lib/libiec61850.so
%{_prefix}/lib/libiec61850.a
%{_prefix}/share/pkgconfig/libiec61850.pc

%files devel
%{_includedir}/libiec61850/*

%changelog
* Mon Nov 6 2017 Steffen Vogel <stvogel@eonerc.rwth-aachen.de
- Initial RPM release
