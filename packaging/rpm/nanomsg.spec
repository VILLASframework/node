# Don't create static libraries (unless we want to)
%bcond_with		static


Name:	 nanomsg
Version: 1.0.0
Release: 40.master.20170523gitg5cc0074%{?dist}
Summary: A fast, scalable, and easy to use socket library

Group:	 System Environment/Libraries
License: MIT
URL:	 http://nanomsg.org/
Source0: nanomsg-%{version}-40-g5cc0074.tar.gz

BuildRequires: rubygem-asciidoctor xmlto cmake
BuildRoot: %(mktemp -ud %{_tmppath}/%{name}-%{version}-%{release}-XXXXXX)


%description
nanomsg is a socket library that provides several common communication
patterns. It aims to make the networking layer fast, scalable, and easy
to use. Implemented in C, it works on a wide range of operating systems
with no further dependencies.

The communication patterns, also called "scalability protocols", are
basic blocks for building distributed systems. By combining them you can
create a vast array of distributed applications.


%if %{with static}
%package	static
Summary:	Libraries for static linking of applications which will use nanomsg
Group:		Development/Libraries
Requires:	%{name}-devel%{?_isa} = %{version}-%{release}

%description	static
nanomsg is a socket library that provides several common communication
patterns. The nanomsg-static package includes static libraries needed to
link and develop applications using this library.

Most users will not need to install this package.
%endif


%package	devel
Summary:	Development files for the nanomsg socket library
Group:		Development/Libraries
Requires:	%{name}%{?_isa} = %{version}-%{release}

%description 	devel
This package contains files needed to develop applications using nanomsg,
a socket library that provides several common communication patterns.


%package	utils
Summary:	Command line interface for communicating with nanomsg
Group:		Applications/Internet

%description	utils
Includes nanocat, a simple utility for reading and writing to nanomsg
sockets and bindings, which can include local and remote connections.


%prep
%setup -q -n nanomsg-1.0.0-40-g5cc0074/

%build
# Enabling static library build disables the dynamic library.
# First configure and build the static library, then reconfigure and build
# the dynamic libs, tools and docs afterwards instead of patching CMake build files
%if %{with static}
	%cmake -DNN_STATIC_LIB=ON -DNN_ENABLE_DOC=OFF -DNN_ENABLE_TEST=OFF -DNN_ENABLE_TOOLS=OFF .
	make %{?_smp_mflags} V=1
%endif
%cmake -DNN_STATIC_LIB=OFF -DNN_ENABLE_DOC=ON -DNN_ENABLE_TEST=ON -DNN_ENABLE_TOOLS=ON .
make %{?_smp_mflags} V=1



%install
rm -fR %{buildroot}
make install DESTDIR="%{buildroot}"


%check
#make test LD_LIBRARY_PATH="%{buildroot}%{_libdir}" DESTDIR="%{buildroot}"


%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig


%clean
rm -fR %{buildroot}


%files
%defattr(-,root,root)
%doc AUTHORS COPYING README.md
%{_libdir}/*.so.*

%if %{with static}
%files static
%defattr(-,root,root)
%{_libdir}/*.a*
%endif


%files devel
%defattr(-,root,root)
%{_docdir}/%{name}
%{_mandir}/man7/*
%{_mandir}/man3/*
%{_includedir}/*
%{_libdir}/*.so
%{_libdir}/pkgconfig/*.pc


%files utils
%defattr(-,root,root)
%{_mandir}/man1/*
%{_bindir}/*


%changelog
* Sat Apr  1 2017 Tarjei Knapstad <tarjei.knapstad@gmail.com> - 1.0.0-1
- Updated to 1.0.0 final release
- nanomsg moved to CMake, so this spec did too
- Changed source URL
- Moved contents of -doc package into -devel
- Removed conditional check, all tests should pass
- The nanocat symlink stuff is gone, nanocat is now a single binary with options

* Tue Oct 27 2015 Japheth Cleaver <cleaver@terabithia.org> 0.7-0.1.beta
- update to 0.7-beta release

* Fri Nov 14 2014 Japheth Cleaver <cleaver@terabithia.org> 0.5-0.1.beta
- update to 0.5-beta release

* Sun Jul 27 2014 Japheth Cleaver <cleaver@terabithia.org> 0.4-0.3.beta
- compile with correct Fedora flags
- move documentation back to base package
- spec file cleanups

* Thu Jul 17 2014 Japheth Cleaver <cleaver@terabithia.org> 0.4-0.2.beta
- drop the 'lib' prefix from package name
- remove explicit pkgconfig requires in nanomsg-devel
- move overview man pages to devel subpackage
- move html to doc subpackage

* Thu Jul 17 2014 Japheth Cleaver <cleaver@terabithia.org> 0.4-0.1.beta
- new "libnanomsg" package based on BZ#1012392, with current versioning
- devel and utils subpackages created, static lib a build conditional
- check section added as a build conditional
- ensure man pages for nanocat symlinks present
- disable RPATH in library
- License set to MIT