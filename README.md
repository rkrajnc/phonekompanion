<!--
SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
SPDX-License-Identifier: CC0-1.0
-->

# Phone Kompanion

[![build result](https://build.opensuse.org/projects/home:rkrajnc11/packages/phonekompanion/badge.svg?type=default)](https://build.opensuse.org/package/show/home:rkrajnc11/phonekompanion)

Phone calls over Bluetooth, the phone's screen, apps and camera, and SMS on the KDE Plasma
desktop, as a companion to [KDE Connect](https://kdeconnect.kde.org/).

![A phone's page, and the dialer searching the contacts](doc/screenshot-dialer.png)

Phone Kompanion uses the phones paired in KDE Connect and adds what KDE Connect does not do by
itself. It runs next to the KDE Connect from your distribution; there is nothing to replace.

## Features

**Calls over Bluetooth**
- Answer, decline and hang up from a notification; the call audio is on the computer, with the
  phone working as a Bluetooth headset gateway (HFP)
- Calls stay on the phone unless you answer them on the computer, or move the audio over
- A dialer with contact search (the contacts KDE Connect syncs), a number pad that also sends
  tones during a call, and microphone mute
- Missed call notifications with Call Back

**The phone's screen, apps and camera**, with [scrcpy](https://github.com/Genymobile/scrcpy)
over Wireless debugging
- Mirror and control the phone's screen in a window
- Open a phone app in its own window, and add phone apps to the application menu
- Show the phone's camera in a window, or use it as a webcam for other apps (v4l2loopback)
- Pair with Wireless debugging from the settings, by QR code or pairing code

**SMS**
- The conversations KDE Connect syncs, read and sent in the same window (KDE Connect's own SMS
  pages, built in)

**Everywhere on the desktop**
- A system tray icon with each phone's calls, messages, mirroring and camera
- KRunner: `call <name or number>`, `phone <app>`, `phone screen`, `phone camera`
- `tel:` and `sms:` links open the dialer or the conversation, with the number and text filled in
- `phonekompanion-cli` for scripts and shortcuts

## Requirements

- KDE Plasma 6, with Qt and KDE Frameworks 6.8 or newer
- KDE Connect, with the phone paired (KDE Connect 26.08 tested)
- For calls: the phone paired over Bluetooth with the computer, BlueZ, and PipeWire with
  WirePlumber, which provides PipeWire's telephony API `org.pipewire.Telephony` (PipeWire 1.6
  tested)
- For mirroring, apps and the camera: `scrcpy` and `adb` (android-tools), and Wireless debugging
  turned on in the phone's Developer options
- For the webcam: the `v4l2loopback` kernel module

Phone Kompanion is developed and tested on openSUSE Tumbleweed with a OnePlus phone.

## Installing

Packages for openSUSE Tumbleweed and Leap 16, Fedora 44, Debian 13 and Ubuntu 26.10 are built
on the openSUSE Build Service:
[install instructions](https://software.opensuse.org/download.html?project=home:rkrajnc11&package=phonekompanion).
On Fedora, scrcpy comes from COPR; on Debian 13, from trixie-backports.

## Setting up

1. Pair the phone in KDE Connect, and pair it over Bluetooth in the system settings.
2. Start Phone Kompanion. Its background service starts with each login from then on.
3. KDE Connect also shows incoming and missed calls. To see each call once, turn off KDE Connect's
   "Incoming call" and "Missed call" notifications; the call settings in Phone Kompanion have a
   button that opens them.
4. For mirroring, turn on Wireless debugging on the phone and pair it once from the mirroring
   settings, with the QR code or the pairing code.
5. To open `tel:` and `sms:` links in Phone Kompanion, also from browsers:

   ```
   xdg-mime default io.github.rkrajnc.phonekompanion.dialer.desktop x-scheme-handler/tel
   xdg-mime default io.github.rkrajnc.phonekompanion.messages.desktop x-scheme-handler/sms
   ```

## Command line

```
phonekompanion-cli --list-phones
phonekompanion-cli --call "Ana"               # or a number
phonekompanion-cli --mirror
phonekompanion-cli --list-apps
phonekompanion-cli --open-app org.example.app
phonekompanion-cli --camera [--camera-facing front]
phonekompanion-cli --webcam | --stop-webcam
```

With more than one phone, choose one with `--phone <id or name>`. `--notify` also shows errors as
a notification, for shortcuts without a terminal.

The window takes `--phone <id>`, `--dialer`, `--messages`, `--apps`, and `tel:` and `sms:` links.

## Building

```
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

Build dependencies: extra-cmake-modules; Qt 6 Core, DBus, Gui, Network, Qml, Quick and Widgets;
KDE Frameworks Config, CoreAddons, DBusAddons, I18n, Notifications, People, StatusNotifierItem
and WindowSystem. Optional: libphonenumber (matches caller numbers to contacts in any format) and
PulseAudioQt (microphone mute). The window needs the QML modules of Kirigami, Kirigami Addons,
Prison and Qt Multimedia.

To install for your user only, without root, use `-DCMAKE_INSTALL_PREFIX=$HOME/.local
-DKDE_INSTALL_AUTOSTARTDIR=$HOME/.config/autostart`.

Packaging for the openSUSE Build Service, for openSUSE, Fedora, Debian and Ubuntu, is in
[packaging/obs](packaging/obs).

## Known limitations

- Muting the phone's ringer from the computer is not possible: KDE Connect does not offer it to
  other apps.
- Without Bluetooth, incoming calls are shown from KDE Connect's information, which does not say
  when the call is answered or ends; the notification closes by itself.
- Clicking KDE Connect's own SMS notifications opens KDE Connect's SMS app, not Phone Kompanion.
- The window is made for the desktop; it has not been tried on Plasma Mobile.

## How it works

`phonekompaniond` is a background service with the tray icon, the KRunner runner and the
notifications. It follows KDE Connect over D-Bus for the phones, their names and whether they are
reachable, and offers its own D-Bus API at `io.github.rkrajnc.phonekompaniond`. Calls go through
PipeWire's telephony API and BlueZ, and mirroring through scrcpy and adb. The window
`phonekompanion` and `phonekompanion-cli` are clients of that service, and the SMS pages talk to
KDE Connect directly.

## License

GPL-2.0-only OR GPL-3.0-only, or any later version accepted by KDE e.V. (see
[LICENSES](LICENSES)). The SMS pages are KDE Connect's code, by its authors, under the same
licence.
