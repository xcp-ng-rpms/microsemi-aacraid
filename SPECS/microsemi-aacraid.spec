%global package_speccommit c3edbd5edfda8bb0f1cc7e620ffb086932c39dd5
%global usver 1.2.1.60001
%global xsver 1
%global xsrel %{xsver}%{?xscount}%{?xshash}
%global package_srccommit 1.2.1.60001

%define vendor_name Microsemi
%define vendor_label microsemi
%define driver_name aacraid

%if %undefined module_dir
%define module_dir updates
%endif

## Keeps rpmlint happy
%{!?kernel_version: %global kernel_version dummy}


Summary: %{vendor_name} %{driver_name} device drivers
Name: %{vendor_label}-%{driver_name}
Version: 1.2.1.60001
Release: %{?xsrel}%{?dist}
License: GPL
Source0: microsemi-aacraid-1.2.1.60001.tar.gz

BuildRequires: kernel-devel
%{?_cov_buildrequires}
Provides: vendor-driver
Requires: kernel-uname-r = %{kernel_version}
Requires(post): /usr/sbin/depmod
Requires(postun): /usr/sbin/depmod


%description
%{vendor_name} %{driver_name} device drivers for the Linux Kernel
version %{kernel_version}


%prep
%setup -n %{name}-%{version}
%{?_cov_prepare}

%build
%{?_cov_wrap} %{make_build} -C /lib/modules/%{kernel_version}/build M=$(pwd) KSRC=/lib/modules/%{kernel_version}/build modules

%install
%{?_cov_wrap} %{__make} %{?_smp_mflags} -C /lib/modules/%{kernel_version}/build M=$(pwd) INSTALL_MOD_PATH=%{buildroot} INSTALL_MOD_DIR=%{module_dir} DEPMOD=/bin/true modules_install

# mark modules executable so that strip-to-file can strip them
find %{buildroot}/lib/modules/%{kernel_version} -name "*.ko" -type f | xargs chmod u+x

%{?_cov_install}

%post
/sbin/depmod %{kernel_version}
%{regenerate_initrd_post}

%postun
/sbin/depmod %{kernel_version}
%{regenerate_initrd_postun}

%posttrans
%{regenerate_initrd_posttrans}

%files
/lib/modules/%{kernel_version}/*/*.ko

%{?_cov_results_package}


%changelog
* Mon Sep 19 2022 Zhuangxuan Fei <zhuangxuan.fei@citrix.com> - 1.2.1.60001-1
- CP-40162: Upgrade microsemi-aacraid driver to version 1.2.1.60001
