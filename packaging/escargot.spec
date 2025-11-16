#
# Copyright (c) 2023-present Samsung Electronics Co., Ltd
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
# USA

Name:          escargot
Summary:       Lightweight JavaScript Engine Escargot
Version:       1.0.0
Release:       1
Group:         Development/Libraries
License:       LGPL-2.1+ and BSD-2-Clause and BSD-3-Clause and BSL-1.0 and MIT and ISC and Zlib and BOEHM-GC and ICU
Source:        %{name}-%{version}.tar.gz
#ExclusiveArch: %arm

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

# RPM ref: http://backreference.org/2011/09/17/some-tips-on-rpm-conditional-macros/

# [ tv | mobile | wearable ]
# The following syntax's been outdated.
# %if %{?tizen_profile_name:1}%{!?tizen_profile_name:0}
# %define profile %{tizen_profile_name}
# %else
# %define profile undefined
# %endif

# [ tv | headless | mobile | wearable | all ]
# build for all profile
%if 0%{?build_profile:1}
%define rpm %{build_profile}
%else
%define rpm all
%endif

%if 0%{?tizen_version_major:1}
%else
%define tizen_version_major 4
%endif

%if 0%{?tizen_version_minor:1}
%else
%define tizen_version_minor 0
%endif

%if %{?_vd_cfg_product_type:1}%{!?_vd_cfg_product_type:0}
  %if "%{_vd_cfg_product_type}" == "AUDIO" || "%{rpm}" == "headless"
%define rpm headless
  %else
    %if "%{_vd_cfg_product_type}" == "TV" || "%{_vd_cfg_product_type}" == "LFD" || "%{_vd_cfg_product_type}" == "IWB" || "%{_vd_cfg_product_type}" == "WALL"
%define rpm prod_tv
    %endif
  %endif
%endif

%if 0%{?sec_product_feature_profile_wearable} == 1
%define rpm wearable
%endif

%if 0%{?rebuild_force:1}
%define force_build 1
%else
%define force_build 0
%endif

%if 0%{?disable_lto:1}
%define using_lto 0
%else
%if (0%{?tizen_version_major} == 5) && (0%{?tizen_version_minor} == 5) && %{?_vd_cfg_product_type:1}%{!?_vd_cfg_product_type:0}
%define using_lto 0
%else
%if 0%{?tizen_version_major} >= 5
%define using_lto 1
%else
%define using_lto 0
%endif
%endif
%endif

%if 0%{?enable_codecache:1}
%else
%define enable_codecache 0
%endif

%if 0%{?enable_wasm:1}
%else
%define enable_wasm 0
%endif

%if 0%{?enable_debugger:1}
%else
%define enable_debugger 0
%endif

%if 0%{?enable_test:1}
%else
%define enable_test 0
%endif

%if 0%{?enable_shell:1}
%else
%define enable_shell 0
%endif

%if 0%{?enable_small_config:1}
%else
%define enable_small_config 0
%endif

# build requirements
BuildRequires: cmake
BuildRequires: ninja
BuildRequires: pkgconfig(dlog)
BuildRequires: pkgconfig(bundle)

# If you want to speed up the gbs build for devel, please uncomment below block.
#%ifarch armv7l
#BuildRequires: clang-accel-armv7l-cross-arm
#%endif # arm7l
#%ifarch aarch64
#BuildRequires: clang-accel-aarch64-cross-aarch64
#%endif # aarch64

# Supporting multiprofiles
# Use profile_mobile as default, as it is both minimal and
# platform-independent version of LWE at the time of writing
# TODO: Creates a profile_common if this is no longer true.
Requires: %{name}-compat = %{version}-%{release}
Recommends: %{name}-profile_mobile = %{version}-%{release}

%description
This package provides implementation of Lightweight JavaScript Engine.


##############################################
# Packages for profiles
##############################################
%if "%{rpm}" == "tv" || "%{rpm}" == "prod_tv" || "%{rpm}" == "all"
%package profile_tv
Summary:     Lightweight JavaScript Engine Escargot for tv
Provides:    %{name}-compat = %{version}-%{release}
Conflicts:   %{name}-profile_headless = %{version}-%{release}
Conflicts:   %{name}-profile_mobile = %{version}-%{release}
Conflicts:   %{name}-profile_wearable = %{version}-%{release}
%description profile_tv
Lightweight JavaScript Engine for tv
%endif

%if "%{rpm}" == "headless"
%package profile_headless
Summary:     Lightweight JavaScript Engine for headless
Provides:    %{name}-compat = %{version}-%{release}
Conflicts:   %{name}-profile_tv = %{version}-%{release}
Conflicts:   %{name}-profile_mobile = %{version}-%{release}
Conflicts:   %{name}-profile_wearable = %{version}-%{release}
%description profile_headless
Lightweight JavaScript Engine for headless
%endif

%if "%{rpm}" == "mobile" || "%{rpm}" == "all"
%package profile_mobile
Summary:     Lightweight JavaScript Engine Escargot for mobile
Provides:    %{name}-compat = %{version}-%{release}
Conflicts:   %{name}-profile_tv = %{version}-%{release}
Conflicts:   %{name}-profile_headless = %{version}-%{release}
Conflicts:   %{name}-profile_wearable = %{version}-%{release}
%description profile_mobile
Lightweight JavaScript Engine for mobile
%endif

%if "%{rpm}" == "wearable" || "%{rpm}" == "all"
%package profile_wearable
Summary:     Lightweight JavaScript Engine Escargot for wearable
Provides:    %{name}-compat = %{version}-%{release}
Conflicts:   %{name}-profile_tv = %{version}-%{release}
Conflicts:   %{name}-profile_headless = %{version}-%{release}
Conflicts:   %{name}-profile_mobile = %{version}-%{release}
%description profile_wearable
Lightweight JavaScript Engine for wearable
%endif

%package devel
Summary:     Development files for Lightweight JavaScript Engine Escargot
Group:       Development/Libraries
Requires:    %{name} = %{version}
%description devel
Development files for Lightweight JavaScript Engine Escargot. This package provides
headers and package configs.

##############################################
# Prep
##############################################
%prep
%setup -q

##############################################
# Build
##############################################
%build
echo "Building for: " %{rpm}

CXXFLAGS+=' -DESCARGOT_TIZEN_MAJOR_VERSION=%{tizen_version_major} '
CXXFLAGS+=' -DESCARGOT_TIZEN_VERSION_%{tizen_version_major}_%{tizen_version_minor} '

##############################################
# Asan with lto leads internal compiler error
##############################################
%if 0%{?asan} == 1
CFLAGS+=' -fno-lto '
CXXFLAGS+=' -fno-lto '
%endif

##############################################
# Disable lto option
##############################################
%if 0%{?using_lto} == 0
CFLAGS+=' -fno-lto '
CXXFLAGS+=' -fno-lto '
%endif

##############################################
## Build rules for each profile
##############################################
%define fp_mode soft
%ifarch armv7l armv7hl
%define tizen_arch arm
%endif
%ifarch armv7hl
%define fp_mode hard
%endif
%ifarch aarch64
%define tizen_arch aarch64
%endif
%ifarch i686
%define tizen_arch i686
%endif
%ifarch x86_64
%define tizen_arch x86_64
%endif
%ifarch riscv64
%define tizen_arch riscv64
%endif

%if "%{rpm}" == "wearable"
CFLAGS+=' -Os '
CXXFLAGS+=' -Os '
%endif

CFLAGS+=' -Wno-shadow '
CXXFLAGS+=' -Wno-shadow '

%if "%{?enable_test}" == "1"
cmake CMakeLists.txt -H./ -Bbuild/out_tizen_%{rpm} -DLIBDIR=%{_libdir} -DINCLUDEDIR=%{_includedir} -DTIZEN_MAJOR_VERSION='%{tizen_version_major}' \
-DESCARGOT_ARCH='%{tizen_arch}' -DESCARGOT_WASM='%{enable_wasm}' -DESCARGOT_DEBUGGER='%{enable_debugger}' -DESCARGOT_SMALL_CONFIG='%{enable_small_config}' \
-DESCARGOT_THREADING=ON -DESCARGOT_TCO=ON -DESCARGOT_MODE=release -DESCARGOT_HOST=tizen -DESCARGOT_OUTPUT=shared_lib -DESCARGOT_TEST=ON -DESCARGOT_TEMPORAL=ON -DESCARGOT_TCO=ON -DESCARGOT_TLS_ACCESS_BY_ADDRESS=ON -G Ninja
%else
cmake CMakeLists.txt -H./ -Bbuild/out_tizen_%{rpm} -DLIBDIR=%{_libdir} -DINCLUDEDIR=%{_includedir} -DTIZEN_MAJOR_VERSION='%{tizen_version_major}' \
-DESCARGOT_ARCH='%{tizen_arch}' -DESCARGOT_WASM='%{enable_wasm}' -DESCARGOT_DEBUGGER='%{enable_debugger}' -DESCARGOT_SMALL_CONFIG='%{enable_small_config}' \
-DESCARGOT_THREADING=ON -DESCARGOT_TLS_ACCESS_BY_ADDRESS=ON -DESCARGOT_MODE=release -DESCARGOT_HOST=tizen -DESCARGOT_OUTPUT=shared_lib -G Ninja
%endif

cmake --build build/out_tizen_%{rpm}

%if "%{?enable_shell}" == "1"

%if "%{?enable_test}" == "1"
CXXFLAGS+=' -DESCARGOT_ENABLE_TEST '
%endif

g++ src/shell/Shell.cpp -std=c++11 -Lbuild/out_tizen_%{rpm} -Isrc/ -Ithird_party/GCutil/include -o build/out_tizen_%{rpm}/escargot -O2 -DNDEBUG -Wl,-rpath=\$ORIGIN -Wl,-rpath=%{_libdir}/escargot ${CXXFLAGS} -lescargot -lpthread
g++ tools/test/test-data-runner/test-data-runner.cpp -o build/out_tizen_%{rpm}/test-data-runner -std=c++11 ${CXXFLAGS} -lpthread
%endif



##############################################
## Install
##############################################

%install
%define bin libescargot.so

rm -rf %{buildroot}
mkdir -p %{buildroot}%{_libdir}
mkdir -p %{buildroot}%{_libdir}/escargot
mkdir -p %{buildroot}%{_bindir}
cp -fr build/out_tizen_%{rpm}/*.so* %{buildroot}%{_libdir}/escargot

# for devel files
mkdir -p %{buildroot}%{_includedir}/%{name}
cp src/api/*.h %{buildroot}%{_includedir}/%{name}

mkdir -p %{buildroot}%{_libdir}/pkgconfig/
cp build/out_tizen_%{rpm}/escargot.pc %{buildroot}%{_libdir}/pkgconfig/
mkdir -p %{buildroot}%{_sysconfdir}/ld.so.conf.d/
cp packaging/escargot.conf %{buildroot}%{_sysconfdir}/ld.so.conf.d/

##############################################
## Scripts
##############################################

# Post Install
%post
/sbin/ldconfig

# Post Uninstall
%postun
/sbin/ldconfig

##############################################
## Packaging rpms
##############################################

%files
%manifest packaging/%{name}.manifest

%if "%{rpm}" == "tv" || "%{rpm}" == "prod_tv" || "%{rpm}" == "all"
%files profile_tv
%manifest packaging/%{name}.manifest
%{_libdir}/escargot/libescargot.so*
%{_sysconfdir}/ld.so.conf.d/*.conf
%license LICENSE.BSD-2-Clause LICENSE.LGPL-2.1+ LICENSE.MPL-2.0 LICENSE.Apache-2.0 LICENSE.BSD-3-Clause LICENSE.MIT LICENSE.BOEHM-GC
%endif

%if "%{rpm}" == "headless"
%files profile_headless
%manifest packaging/%{name}.manifest
%{_libdir}/escargot/libescargot.so*
%{_sysconfdir}/ld.so.conf.d/*.conf
%license LICENSE.BSD-2-Clause LICENSE.LGPL-2.1+ LICENSE.MPL-2.0 LICENSE.Apache-2.0 LICENSE.BSD-3-Clause LICENSE.MIT LICENSE.BOEHM-GC
%endif

%if "%{rpm}" == "mobile" || "%{rpm}" == "all"
%files profile_mobile
%manifest packaging/%{name}.manifest
%{_libdir}/escargot/libescargot.so*
%{_sysconfdir}/ld.so.conf.d/*.conf
%license LICENSE.BSD-2-Clause LICENSE.LGPL-2.1+ LICENSE.MPL-2.0 LICENSE.Apache-2.0 LICENSE.BSD-3-Clause LICENSE.MIT LICENSE.BOEHM-GC
%endif

%if "%{rpm}" == "wearable" || "%{rpm}" == "all"
%files profile_wearable
%manifest packaging/%{name}.manifest
%{_libdir}/escargot/libescargot.so*
%{_sysconfdir}/ld.so.conf.d/*.conf
%license LICENSE.BSD-2-Clause LICENSE.LGPL-2.1+ LICENSE.MPL-2.0 LICENSE.Apache-2.0 LICENSE.BSD-3-Clause LICENSE.MIT LICENSE.BOEHM-GC
%endif

%files devel
%manifest packaging/%{name}.manifest
%{_includedir}
%{_libdir}/pkgconfig/*.pc
