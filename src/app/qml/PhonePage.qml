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
import io.github.rkrajnc.phonekompanion.sms as SMS

Kirigami.ScrollablePage {
    id: page

    required property string phoneId
    required property string phoneName
    readonly property alias scrcpy: scrcpy
    readonly property alias calls: calls
    readonly property bool ready: scrcpy.available && !scrcpy.unavailableReason

    title: phone.name || phoneName

    // The page last opened next to this one, by name, for forward to reopen it after back closed it
    property string subpage
    // --messages before the phone's status was read
    property bool messagesWanted: false

    // Replaces the page next to this one: "apps", "dialer", "messages", "callsSettings" or "scrcpySettings"
    function openSubpage(name: string): Kirigami.Page {
        const components = {
            apps: appsPage,
            dialer: dialerPage,
            messages: messagesPage,
            callsSettings: callsSettingsPage,
            scrcpySettings: scrcpySettingsPage
        };
        if (!canOpen(name)) {
            return null;
        }
        // Another page opened meanwhile wins over a waiting --messages
        if (name !== "messages") {
            messagesWanted = false;
        }
        subpage = name;
        pageStack.pop(page);
        return pageStack.push(components[name]);
    }

    // As the entries here allow: Messages only while KDE Connect's SMS plugin is there, as the
    // conversations load only then, and the apps only while mirroring is possible
    function canOpen(name: string): bool {
        if (name === "messages") {
            return phone.messagesAvailable;
        } else if (name === "apps") {
            return ready;
        }
        return true;
    }

    function openApps() {
        return openSubpage("apps");
    }

    function openDialer() {
        return openSubpage("dialer");
    }

    function openMessages() {
        // A page just opened has not read the phone's status yet
        messagesWanted = !phone.available;
        return openSubpage("messages");
    }

    actions: [
        Kirigami.Action {
            icon.name: "configure"
            text: i18n("Settings")

            Kirigami.Action {
                icon.name: "call-start"
                text: i18n("Calls")
                onTriggered: page.openSubpage("callsSettings")
            }
            Kirigami.Action {
                icon.name: "video-display"
                text: i18n("Screen Mirroring")
                onTriggered: page.openSubpage("scrcpySettings")
            }
        }
    ]

    PhoneInterface {
        id: phone
        phoneId: page.phoneId
        onStatusChanged: {
            if (page.messagesWanted && available) {
                page.messagesWanted = false;
                page.openMessages();
            }
        }
    }

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
            onSettingsRequested: page.openSubpage("callsSettings")
        }
    }

    // KDE Connect's SMS pages, which open the conversations themselves
    Component {
        id: messagesPage
        // Opened only while KDE Connect's SMS plugin is there, as the conversations load only then
        SMS.ConversationList {
            id: messagesList

            title: i18n("Messages")
            deviceId: page.phoneId
            deviceConnected: phone.messagesAvailable

            // Back, e.g. after the phone left the network: KDE Connect does not send the list by itself
            Connections {
                target: phone
                function onMessagesAvailableChanged() {
                    if (phone.messagesAvailable) {
                        messagesList.refresh();
                    }
                }
            }
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
            text: i18n("Messages")
        }

        KD.SubtitleDelegate {
            Layout.fillWidth: true
            icon.name: "dialog-messages"
            text: i18n("Messages")
            subtitle: {
                if (phone.messagesAvailable) {
                    return i18n("Read and send SMS through KDE Connect");
                } else if (phone.reachable) {
                    return i18n("SMS is not available for this phone in KDE Connect");
                }
                return i18n("Needs the phone connected to KDE Connect");
            }
            enabled: phone.messagesAvailable
            onClicked: page.openMessages()
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
                onTriggered: page.openSubpage("scrcpySettings")
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

        Kirigami.ListSectionHeader {
            Layout.fillWidth: true
            text: i18n("Camera")
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
