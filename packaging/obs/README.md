<!--
SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
SPDX-License-Identifier: CC0-1.0
-->

# Packaging on the openSUSE Build Service

`phonekompanion.spec` and `phonekompanion.changes` build an RPM for openSUSE Tumbleweed
(and Slowroll). It needs Qt and KDE Frameworks 6.8 or newer.

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
osc service manualrun                         # downloads the tarball of the tag
osc add *
osc commit -m "Initial package"
```

In the project's repositories, add openSUSE Tumbleweed (`osc meta prj -e home:<user>`, or
Repositories in the web interface). OBS rebuilds the package when Tumbleweed's Qt changes; the
compiled QML uses Qt's private API, so the package fits only the Qt it was built against.

## New versions

Raise `project(... VERSION)` in `CMakeLists.txt`, `Version:` in the spec and the release in the
AppStream file, tag and push `v<version>`, add an entry with `osc vc`, then `osc rm` the old
tarball, `osc service manualrun`, `osc add` the new one and `osc commit`.

## Checking a build locally

`osc build openSUSE_Tumbleweed x86_64` builds in a clean chroot. A quicker check against the
installed system, without osc, from a tarball of HEAD named as GitHub's would be:

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
