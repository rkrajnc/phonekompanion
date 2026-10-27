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
# GitHub's tarball of the release tag v<version>; `osc service manualrun` downloads it
Source0:        %{url}/archive/refs/tags/v%{version}/%{name}-%{version}.tar.gz
BuildRequires:  kf6-extra-cmake-modules >= %{kf6_version}
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
# Calls over Bluetooth: PipeWire's telephony API (org.pipewire.Telephony) and BlueZ
Recommends:     bluez
Recommends:     pipewire
Recommends:     wireplumber
# The phone's screen, apps and camera
Recommends:     android-tools
Recommends:     scrcpy
# The phone's camera as a webcam
Suggests:       v4l2loopback-kmp

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
%kf6_build

%install
%kf6_install

%files
%license LICENSES/*
%{_kf6_bindir}/phonekompanion
%{_kf6_bindir}/phonekompanion-cli
%{_kf6_bindir}/phonekompaniond
%{_kf6_applicationsdir}/io.github.rkrajnc.phonekompanion.desktop
%{_kf6_applicationsdir}/io.github.rkrajnc.phonekompanion.dialer.desktop
%{_kf6_applicationsdir}/io.github.rkrajnc.phonekompanion.messages.desktop
%{_kf6_applicationsdir}/io.github.rkrajnc.phonekompaniond.desktop
%{_kf6_appstreamdir}/io.github.rkrajnc.phonekompanion.metainfo.xml
%{_kf6_configdir}/autostart/io.github.rkrajnc.phonekompaniond.desktop
%{_kf6_debugdir}/phonekompanion.categories
%{_kf6_notificationsdir}/phonekompaniond.notifyrc
%dir %{_kf6_sharedir}/krunner
%dir %{_kf6_sharedir}/krunner/dbusplugins
%{_kf6_sharedir}/krunner/dbusplugins/phonekompanion_runner.desktop
%{_kf6_sharedir}/dbus-1/services/io.github.rkrajnc.phonekompaniond.service

%changelog
