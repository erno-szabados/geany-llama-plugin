Name:           geany-llama-plugin
Version:        0.1
Release:        0
Summary:        LLM plugin for Geany
License:        GPL-3.0-or-later
Group:          Productivity/Editors/Geany
URL:            https://github.com/yourusername/geany-llama-plugin
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  geany-devel
BuildRequires:  gtk3-devel
BuildRequires:  glib2-devel
BuildRequires:  libcurl-devel
BuildRequires:  intltool
BuildRequires:  gcc
BuildRequires:  make
BuildRequires:  libjson-c-devel
Requires:       geany
Requires:       libjson-c5
Requires:       libcurl4

%description
A plugin for Geany that integrates LLM features.

%prep
%setup -q

%build
./configure --prefix=%{_prefix}
make %{?_smp_mflags}

%install
make DESTDIR=%{buildroot} install

%files
%license COPYING
%doc README.md
%{_libdir}/geany/libgeany_llm.so*
%{_datadir}/locale/*/LC_MESSAGES/geany-llm-plugin.mo
%exclude %{_libdir}/geany/libgeany_llm.a
%exclude %{_libdir}/geany/libgeany_llm.la

%changelog
* Sat May 17 2025 Erno Szabados <erno.szabados@windowslive.com> - 0.1-0
- Initial package