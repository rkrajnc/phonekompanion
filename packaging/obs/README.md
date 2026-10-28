<!--
SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
SPDX-License-Identifier: CC0-1.0
-->

# Packaging on the openSUSE Build Service

The files here build packages for:

- openSUSE Tumbleweed (and Slowroll), Leap 16 and Fedora: `phonekompanion.spec`, with
  `phonekompanion.changes` for openSUSE's changelog
- Debian 13 and newer, and Ubuntu 25.10 and newer: `phonekompanion.dsc` and the `debian.*`
  files. OBS's debtransform turns `debian.<name>` into `debian/<name>` and the tarball into the
  `.orig.tar.gz`. OBS reads the build dependencies from the `.dsc`, so they must stay the same as
  in `debian.control`.

It needs Qt and KDE Frameworks 6.8 or newer, so openSUSE Leap 15 and Ubuntu 24.04 cannot build it.
On Fedora, scrcpy comes from COPR and v4l2loopback from RPM Fusion; on Debian 13, scrcpy is in
trixie-backports (contrib).

## The source tarball

OBS takes GitHub's tarball of a release tag, `v<version>`, as the spec's `Source0` says. Tag the
release and push the tag first:

```
git tag -a v0.1.0 -m "Phone Kompanion 0.1.0"
git push origin v0.1.0
```

## First upload

```
sudo zypper install osc
osc checkout home:<user>                      # asks for the openSUSE account on first use
cd home:<user>
osc mkpac phonekompanion
cd phonekompanion
cp <repo>/packaging/obs/{phonekompanion.spec,phonekompanion.changes,_service} .
cp <repo>/packaging/obs/{phonekompanion.dsc,debian.*} .
osc service manualrun                         # downloads the tarball of the tag
osc add *
osc commit -m "Initial package"
```

In the project's repositories, add the distributions (`osc meta prj -e home:<user>`, or
Repositories in the web interface). OBS rebuilds the package when a distribution's Qt changes;
the compiled QML uses Qt's private API, so the package fits only the Qt it was built against.

## New versions

Raise `project(... VERSION)` in `CMakeLists.txt`, `Version:` in the spec and the release in the
AppStream file. For Debian, raise `Version:` and `DEBTRANSFORM-TAR:` in `phonekompanion.dsc` and
add an entry to `debian.changelog` (`<version>-1`). Tag and push `v<version>`, add an entry with
`osc vc`, then `osc rm` the old tarball, `osc service manualrun`, `osc add` the new one and
`osc commit`.

## Checking a build locally

`osc build openSUSE_Tumbleweed x86_64` (or `Fedora_44`, `Debian_13`, ...) builds in a clean
chroot. A quicker check against the installed system, without osc, from a tarball of HEAD named
as GitHub's would be:

```
VERSION=0.1.0
T=$(mktemp -d); mkdir -p $T/SOURCES $T/SPECS
git archive --prefix=phonekompanion-$VERSION/ -o $T/SOURCES/phonekompanion-$VERSION.tar.gz HEAD
cp packaging/obs/phonekompanion.spec $T/SPECS/
rpmbuild -ba --define "_topdir $T" $T/SPECS/phonekompanion.spec
```

## After installing

The service starts at the next login, or at once with `phonekompaniond &`. KDE Connect's own
"Incoming call" and "Missed call" notifications are best turned off (Phone Kompanion's call
settings have a button for that). For tel: and sms: links in browsers:

```
xdg-mime default io.github.rkrajnc.phonekompanion.dialer.desktop x-scheme-handler/tel
xdg-mime default io.github.rkrajnc.phonekompanion.messages.desktop x-scheme-handler/sms
```
