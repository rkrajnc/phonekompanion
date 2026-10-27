/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import io.github.rkrajnc.phonekompanion

Kirigami.ScrollablePage {
    id: page

    required property string phoneId
    required property string phoneName
    readonly property alias scrcpy: scrcpy
    readonly property alias calls: calls
    readonly property bool ready: scrcpy.available && !scrcpy.unavailableReason

    title: phoneName

    // Replaces the page next to this one
    function openSubpage(component: Component): Kirigami.Page {
        pageStack.pop(page);
        return pageStack.push(component);
    }

    function openApps() {
        return openSubpage(appsPage);
    }

    function openDialer() {
        return openSubpage(dialerPage);
    }

    actions: [
        Kirigami.Action {
            icon.name: "configure"
            text: i18n("Settings")

            Kirigami.Action {
                icon.name: "call-start"
                text: i18n("Calls")
                onTriggered: page.openSubpage(callsSettingsPage)
            }
            Kirigami.Action {
                icon.name: "video-display"
                text: i18n("Screen Mirroring")
                onTriggered: page.openSubpage(scrcpySettingsPage)
            }
        }
    ]

    ScrcpyInterface {
        id: scrcpy
        phoneId: page.phoneId
        onError: message => applicationWindow().showPassiveNotification(message)
    }

    CallsInterface {
        id: calls
        phoneId: page.phoneId
        onError: message => applicationWindow().showPassiveNotification(message)
    }

    Component {
        id: appsPage
        AppsPage {
            scrcpy: page.scrcpy
        }
    }

    Component {
        id: scrcpySettingsPage
        ScrcpySettingsPage {
            scrcpy: page.scrcpy
        }
    }

    Component {
        id: dialerPage
        DialerPage {
            calls: page.calls
            phoneName: page.phoneName
            onSettingsRequested: page.openSubpage(callsSettingsPage)
        }
    }

    Component {
        id: callsSettingsPage
        CallsSettingsPage {
            calls: page.calls
        }
    }

    ColumnLayout {
        spacing: 0

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Calls")
        }

        KD.SubtitleDelegate {
            Layout.fillWidth: true
            icon.name: "call-start"
            text: i18n("Phone Calls")
            subtitle: {
                const caller = calls.callName || calls.callNumber || i18n("unknown number");
                if (calls.callState === "incoming" || calls.callState === "waiting") {
                    return i18n("Incoming call from %1", caller);
                } else if (calls.callState !== "") {
                    return i18n("In a call with %1", caller);
                } else if (!calls.available || calls.gatewayConnected) {
                    return i18n("Call numbers and contacts through the phone");
                } else if (!calls.telephonyAvailable) {
                    return i18n("Needs PipeWire with telephony support");
                } else if (calls.settings.useThisComputer === false) {
                    return i18n("Turned off for this computer");
                }
                return i18n("Not connected over Bluetooth");
            }
            onClicked: page.openDialer()
        }

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Screen Mirroring")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            Layout.margins: Kirigami.Units.largeSpacing
            visible: text !== ""
            type: scrcpy.missingTools.length > 0 ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
            text: scrcpy.unavailableReason
            actions: Kirigami.Action {
                visible: scrcpy.available && scrcpy.missingTools.length === 0 && scrcpy.networkReachable
                icon.name: "network-connect"
                text: i18n("Pair…")
                onTriggered: page.openSubpage(scrcpySettingsPage)
            }
        }

        KD.SubtitleDelegate {
            Layout.fillWidth: true
            icon.name: "video-display"
            text: i18n("Mirror Screen")
            subtitle: i18n("Show and control the phone's screen in a window")
            enabled: page.ready
            onClicked: scrcpy.mirrorScreen()
        }

        KD.SubtitleDelegate {
            Layout.fillWidth: true
            icon.name: "view-app-grid"
            text: i18n("Open App…")
            subtitle: i18n("Open a phone app in its own window")
            enabled: page.ready
            onClicked: page.openApps()
        }

        KD.SubtitleDelegate {
            Layout.fillWidth: true
            icon.name: "camera-photo"
            text: i18n("Show Camera")
            subtitle: i18n("Show the phone's camera in a window")
            enabled: page.ready
            onClicked: scrcpy.showCamera()
        }

        KD.SubtitleDelegate {
            Layout.fillWidth: true
            icon.name: "camera-web"
            text: scrcpy.webcamActive ? i18n("Stop Webcam") : i18n("Use Camera as Webcam")
            subtitle: scrcpy.webcamDevice ? i18n("Other apps see the phone's camera as \"%1\"", scrcpy.webcamName) : i18n("Needs the v4l2loopback kernel module")
            // Also without a webcam device, as starting looks for it again
            enabled: scrcpy.webcamActive || page.ready
            onClicked: scrcpy.webcamActive ? scrcpy.stopWebcam() : scrcpy.startWebcam()
        }
    }
}
