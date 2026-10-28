#
# spec file for package phonekompanion
#
# SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
# SPDX-License-Identifier: CC0-1.0
#

%define kf6_version 6.8.0
%define qt6_version 6.8.0

Name:           phonekompanion
Version:        0.1.0
Release:        0
Summary:        Calls, phone apps and SMS on the desktop, next to KDE Connect
License:        GPL-2.0-only OR GPL-3.0-only
URL:            https://github.com/rkrajnc/phonekompanion
# The release tag's source, from the _service on OBS, which also sets Version
Source0:        %{name}-%{version}.tar.gz
BuildRequires:  gcc-c++
BuildRequires:  libphonenumber-devel
BuildRequires:  cmake(KF6Config) >= %{kf6_version}
BuildRequires:  cmake(KF6CoreAddons) >= %{kf6_version}
BuildRequires:  cmake(KF6DBusAddons) >= %{kf6_version}
BuildRequires:  cmake(KF6I18n) >= %{kf6_version}
BuildRequires:  cmake(KF6Notifications) >= %{kf6_version}
BuildRequires:  cmake(KF6People) >= %{kf6_version}
BuildRequires:  cmake(KF6PulseAudioQt)
BuildRequires:  cmake(KF6StatusNotifierItem) >= %{kf6_version}
BuildRequires:  cmake(KF6WindowSystem) >= %{kf6_version}
BuildRequires:  cmake(Qt6Core) >= %{qt6_version}
BuildRequires:  cmake(Qt6DBus) >= %{qt6_version}
BuildRequires:  cmake(Qt6Gui) >= %{qt6_version}
BuildRequires:  cmake(Qt6Network) >= %{qt6_version}
BuildRequires:  cmake(Qt6Qml) >= %{qt6_version}
BuildRequires:  cmake(Qt6Quick) >= %{qt6_version}
BuildRequires:  cmake(Qt6Widgets) >= %{qt6_version}
# Calls over Bluetooth: PipeWire's telephony API (org.pipewire.Telephony) and BlueZ
Recommends:     bluez
Recommends:     pipewire
Recommends:     wireplumber
# The phone's screen, apps and camera; on Fedora, scrcpy is in COPR
Recommends:     android-tools
Recommends:     scrcpy
%if 0%{?suse_version}
BuildRequires:  kf6-extra-cmake-modules >= %{kf6_version}
# The QML modules are checked for at configure time
BuildRequires:  qt6qmlimport(QtMultimedia)
BuildRequires:  qt6qmlimport(org.kde.kirigami)
BuildRequires:  qt6qmlimport(org.kde.kirigamiaddons.components)
BuildRequires:  qt6qmlimport(org.kde.prison)
# Phones and their SMS, contacts and call notifications come from KDE Connect
Requires:       kdeconnect-kde
Requires:       qt6qmlimport(QtMultimedia)
Requires:       qt6qmlimport(org.kde.kirigami)
Requires:       qt6qmlimport(org.kde.kirigamiaddons.components)
Requires:       qt6qmlimport(org.kde.prison)
# The phone's camera as a webcam
Suggests:       v4l2loopback-kmp
%else
BuildRequires:  extra-cmake-modules >= %{kf6_version}
BuildRequires:  kf6-rpm-macros
# libphonenumber's headers include protobuf's, which libphonenumber-devel does not pull in
BuildRequires:  protobuf-devel
# The QML modules are checked for at configure time
BuildRequires:  kf6-kirigami
BuildRequires:  kf6-kirigami-addons
BuildRequires:  kf6-prison
BuildRequires:  qt6-qtmultimedia
# Phones and their SMS, contacts and call notifications come from KDE Connect
Requires:       kde-connect
Requires:       kf6-kirigami
Requires:       kf6-kirigami-addons
Requires:       kf6-prison
Requires:       qt6-qtmultimedia
# The phone's camera as a webcam, from RPM Fusion
Suggests:       v4l2loopback
%endif

%description
Phone Kompanion works alongside KDE Connect and uses the phones paired
there. It adds phone calls over Bluetooth with the call audio on the
computer, the phone's screen, apps and camera in windows (with scrcpy and
Wireless debugging), and the SMS conversations KDE Connect syncs, with
entries in the system tray and KRunner, a command line tool, and tel: and
sms: links.

%prep
%autosetup -p1

%build
%cmake_kf6
%if 0%{?suse_version}
%kf6_build
%else
%cmake_build
%endif

%install
%if 0%{?suse_version}
%kf6_install
%else
%cmake_install
%endif

%files
%license LICENSES/*
%{_bindir}/phonekompanion
%{_bindir}/phonekompanion-cli
%{_bindir}/phonekompaniond
%{_datadir}/applications/io.github.rkrajnc.phonekompanion.desktop
%{_datadir}/applications/io.github.rkrajnc.phonekompanion.dialer.desktop
%{_datadir}/applications/io.github.rkrajnc.phonekompanion.messages.desktop
%{_datadir}/applications/io.github.rkrajnc.phonekompaniond.desktop
%{_datadir}/metainfo/io.github.rkrajnc.phonekompanion.metainfo.xml
%{_sysconfdir}/xdg/autostart/io.github.rkrajnc.phonekompaniond.desktop
%{_datadir}/qlogging-categories6/phonekompanion.categories
%{_datadir}/knotifications6/phonekompaniond.notifyrc
%dir %{_datadir}/krunner
%dir %{_datadir}/krunner/dbusplugins
%{_datadir}/krunner/dbusplugins/phonekompanion_runner.desktop
%{_datadir}/dbus-1/services/io.github.rkrajnc.phonekompaniond.service

%changelog
