%global plas_version 0.1.0

Name:           plas
Version:        %{plas_version}
Release:        1%{?dist}
Summary:        Platform Library Across Systems
License:        Proprietary
URL:            https://github.com/example/plas

Source0:        %{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.16
BuildRequires:  gcc-c++ >= 7
BuildRequires:  make

%description
PLAS (Platform Library Across Systems) provides a unified C++17 interface
for hardware backends including I2C adapters, power management units,
and SSD GPIO control.

# Meta package - installs everything
%package -n plas
Summary:        PLAS complete installation (meta-package)
Requires:       plas-core = %{version}-%{release}
Requires:       plas-devel = %{version}-%{release}

# Core libraries
%package core
Summary:        PLAS core libraries
Provides:       libplas_core.so
Provides:       libplas_log.so
Provides:       libplas_config.so
Provides:       libplas_backend_interface.so
Provides:       libplas_backend_driver.so

%description core
Core PLAS libraries including logging, configuration parsing,
device interface definitions, and driver implementations.

# Development headers
%package devel
Summary:        PLAS development headers and CMake config
Requires:       plas-core = %{version}-%{release}

%description devel
Header files and CMake configuration for building applications
against the PLAS libraries.

# Future packages (placeholders)
# %package xpmu
# Summary:        PLAS PMU library and CLI tools
# Requires:       plas-core = %{version}-%{release}
# %description xpmu
# PMU-related functionality built on the PLAS backend.

# %package xsideband
# Summary:        PLAS sideband library and CLI tools
# Requires:       plas-core = %{version}-%{release}
# %description xsideband
# Sideband communication library using I2C, I3C, and DOC transports.

%prep
%autosetup -n %{name}-%{version}

%build
%cmake -DPLAS_BUILD_TESTS=OFF -DPLAS_INSTALL=ON
%cmake_build

%install
%cmake_install

%files
# Meta package has no files of its own

%files core
%{_libdir}/libplas_core.so*
%{_libdir}/libplas_log.so*
%{_libdir}/libplas_config.so*
%{_libdir}/libplas_backend_interface.so*
%{_libdir}/libplas_backend_driver.so*
%{_datadir}/plas/VERSION

%files devel
%{_includedir}/plas/
%{_libdir}/cmake/plas/

%changelog
* Sat Feb 28 2026 PLAS Team <plas@example.com> - 0.1.0-1
- Initial package
