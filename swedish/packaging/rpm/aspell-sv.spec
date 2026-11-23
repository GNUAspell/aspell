# The RWS hash is native-architecture data, not an ELF binary.
%global debug_package %{nil}

Name:           aspell-sv
Epoch:          50
Version:        2026.11.23
Release:        0.1.rc1%{?dist}
Summary:        Swedish dictionary preview for GNU Aspell
License:        GPL-2.0-or-later AND LGPL-2.1-only AND LGPL-3.0-only
URL:            https://github.com/yeager/aspell-sv
Source0:        %{url}/releases/download/sv-v%{version}-rc1/aspell-sv-%{version}.tar.gz
BuildRequires:  aspell
BuildRequires:  aspell-devel
BuildRequires:  python3
BuildRequires:  make
Requires:       aspell%{?_isa} >= 0.60.8

%description
Swedish word forms derived from the original Aspell list and an expanded
Hunspell source. This preview disables free compound joining to avoid known
false positives. Correct unseen compounds may therefore be rejected.

%prep
%setup -q

%build
%make_build

%install
%make_install dictdir=%{_libdir}/aspell-0.60 datadir=%{_libdir}/aspell-0.60

%check
make check

%files
%license COPYING COPYING.GPL2 COPYING.LESSER COPYING.aspell Copyright.aspell LICENSE.hunspell
%doc README.md stats.json source-sha256.json docs
%{_libdir}/aspell-0.60/sv.rws
%{_libdir}/aspell-0.60/sv.multi
%{_libdir}/aspell-0.60/sv.dat
%{_libdir}/aspell-0.60/sv_phonet.dat

%changelog
* Mon Nov 23 2026 Daniel Nylander <github@danielnylander.se> - 50:2026.11.23-0.1.rc1
- Refresh the dictionary from hunspell-sv 2abdb04

* Mon Sep 21 2026 Daniel Nylander <github@danielnylander.se> - 50:2026.09.21-0.3.rc3
- Enforce Sprakradet source policy and package the reviewed rule catalogue

* Mon Sep 21 2026 Daniel Nylander <github@danielnylander.se> - 50:2026.09.21-0.2.rc2
- Correct reviewed modern and legacy spellings and extend whole-word tests

* Thu Sep 17 2026 Daniel Nylander <github@danielnylander.se> - 50:2026.09.17-0.1.rc1
- Package the Swedish dictionary conversion preview
- Validate every compiled word form and compound spelling regressions
