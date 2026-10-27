<!--
SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
SPDX-License-Identifier: CC0-1.0
-->

# Packaging on the openSUSE Build Service

`phonekompanion.spec` and `phonekompanion.changes` build an RPM for openSUSE Tumbleweed
(and Slowroll). It needs Qt and KDE Frameworks 6.8 or newer.

## The source tarball

The repository is not public yet, so OBS gets a tarball made from a commit:

```
VERSION=0.1.0
git archive --prefix=phonekompanion-$VERSION/ -o phonekompanion-$VERSION.tar.xz HEAD
```

Once the repository is public, a `_service` file with `obs_scm` (or `download_url`) can fetch it
instead, and the spec gets a `URL:`. The AppStream file then also wants a homepage
(`<url type="homepage">`); `appstreamcli validate` warns about it until then.

## First upload

```
sudo zypper install osc
osc checkout home:<user>                      # asks for the openSUSE account on first use
cd home:<user>
osc mkpac phonekompanion
cd phonekompanion
cp <repo>/packaging/obs/phonekompanion.spec <repo>/packaging/obs/phonekompanion.changes .
cp <tarball> .
osc add *
osc commit -m "Initial package"
```

In the project's repositories, add openSUSE Tumbleweed (`osc meta prj -e home:<user>`, or
Repositories in the web interface). OBS rebuilds the package when Tumbleweed's Qt changes; the
compiled QML uses Qt's private API, so the package fits only the Qt it was built against.

## New versions

Raise `project(... VERSION)` in `CMakeLists.txt`, `Version:` in the spec and the release in the
AppStream file, add an entry with `osc vc`, replace the tarball, `osc commit`.

## Checking a build locally

`osc build openSUSE_Tumbleweed x86_64` builds in a clean chroot. A quicker check against the
installed system, without osc:

```
T=$(mktemp -d); mkdir -p $T/SOURCES $T/SPECS
cp phonekompanion-$VERSION.tar.xz $T/SOURCES/ && cp packaging/obs/phonekompanion.spec $T/SPECS/
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
