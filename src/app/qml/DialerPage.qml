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
import io.github.rkrajnc.phonekompanion

Kirigami.ScrollablePage {
    id: page

    required property CallsInterface calls
    required property string phoneName

    readonly property string callState: calls.callState
    readonly property bool inCall: callState !== ""
    readonly property bool ringing: callState === "incoming" || callState === "waiting"
    readonly property bool canCall: calls.gatewayConnected
    readonly property bool queryIsNumber: /^[0-9+*#()\/. -]*[0-9][0-9+*#()\/. -]*$/.test(query.text)
    // The contacts found for the text as it is now, not for an earlier one
    readonly property bool contactsCurrent: calls.contactsQuery === query.text

    signal settingsRequested

    title: i18n("Phone Calls")

    function pressKey(key: string) {
        if (inCall) {
            calls.sendTones(key);
        } else {
            query.insert(query.cursorPosition, key);
            query.forceActiveFocus();
        }
    }

    actions: [
        Kirigami.Action {
            icon.name: "configure"
            text: i18n("Settings")
            onTriggered: page.settingsRequested()
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !page.canCall
            text: {
                if (!page.calls.available) {
                    return i18n("Calls work while the Phone Kompanion service is running.");
                } else if (!page.calls.telephonyAvailable) {
                    return i18n("Calls over Bluetooth need PipeWire with telephony support (org.pipewire.Telephony), which is not running.");
                }
                return i18n("%1 is not connected to this computer over Bluetooth, or calls are turned off for this computer in the settings.", page.phoneName);
            }
            actions: Kirigami.Action {
                icon.name: "configure"
                text: i18n("Settings")
                onTriggered: page.settingsRequested()
            }
        }

        // During a call
        ColumnLayout {
            Layout.fillWidth: true
            visible: page.inCall

            Kirigami.Heading {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: page.calls.callName || page.calls.callNumber || i18n("Unknown number")
            }

            QQC2.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                visible: page.calls.callName !== ""
                text: page.calls.callNumber
            }

            QQC2.Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                opacity: 0.7
                text: {
                    if (page.ringing) {
                        return i18n("Incoming call");
                    } else if (page.callState === "dialing" || page.callState === "alerting") {
                        return i18n("Calling…");
                    } else if (page.callState === "held") {
                        return i18n("On hold");
                    } else if (page.calls.audioOnPc) {
                        return i18n("In call");
                    }
                    return i18n("In call, audio on the phone");
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter

                QQC2.Button {
                    visible: page.ringing
                    icon.name: "call-start"
                    text: i18n("Answer")
                    onClicked: page.calls.answer()
                }

                QQC2.Button {
                    visible: page.callState === "active" && !page.calls.audioOnPc
                    icon.name: "audio-headset"
                    text: i18n("Move Audio to PC")
                    onClicked: page.calls.moveAudioToPc()
                }

                QQC2.Button {
                    visible: page.callState === "active" && page.calls.audioOnPc && page.calls.microphoneMuteSupported
                    icon.name: page.calls.microphoneMuted ? "microphone-sensitivity-muted" : "microphone-sensitivity-high"
                    text: page.calls.microphoneMuted ? i18n("Unmute") : i18n("Mute")
                    onClicked: page.calls.setMicrophoneMuted(!page.calls.microphoneMuted)
                }

                QQC2.Button {
                    icon.name: "call-stop"
                    text: page.ringing ? i18n("Decline") : i18n("Hang Up")
                    onClicked: page.calls.hangUp()
                }
            }
        }

        // Before a call
        QQC2.TextField {
            id: query
            Layout.fillWidth: true
            visible: !page.inCall
            enabled: page.canCall
            placeholderText: i18n("Name or phone number")
            focus: true
            onTextChanged: searchTimer.restart()
            onAccepted: {
                if (page.queryIsNumber) {
                    page.calls.dial(text);
                } else if (page.contactsCurrent && page.calls.contacts.length === 1) {
                    page.calls.dial(page.calls.contacts[0].number);
                }
            }

            // Searches once typing pauses, not on every key
            Timer {
                id: searchTimer
                interval: 200
                onTriggered: page.calls.searchContacts(page.queryIsNumber ? "" : query.text)
            }
        }

        Repeater {
            model: page.inCall ? [] : (page.queryIsNumber ? [{
                        name: "",
                        number: query.text
                    }] : page.calls.contacts)

            delegate: QQC2.ItemDelegate {
                id: contactDelegate

                required property var modelData

                Layout.fillWidth: true
                enabled: page.canCall
                icon.name: "call-start"
                text: i18n("Call %1", modelData.name || modelData.number)
                onClicked: page.calls.dial(modelData.number)

                QQC2.Label {
                    anchors.right: parent.right
                    anchors.rightMargin: Kirigami.Units.largeSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    visible: contactDelegate.modelData.name !== ""
                    opacity: 0.7
                    text: contactDelegate.modelData.number
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            visible: !page.inCall && !page.queryIsNumber && query.text.trim() !== "" && page.contactsCurrent && page.calls.contacts.length === 0
            opacity: 0.7
            text: i18n("No matching contacts")
        }

        // Number pad: types the number, or sends tones during a call
        GridLayout {
            Layout.alignment: Qt.AlignHCenter
            columns: 3
            // Tones can only be sent during an active call
            enabled: page.canCall && (!page.inCall || page.callState === "active")

            Repeater {
                model: ["1", "2", "3", "4", "5", "6", "7", "8", "9", "*", "0", "#"]

                delegate: QQC2.Button {
                    required property string modelData

                    Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                    text: modelData
                    onClicked: page.pressKey(modelData)
                }
            }

            QQC2.Button {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                visible: !page.inCall
                text: "+"
                onClicked: page.pressKey("+")
            }

            QQC2.Button {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                visible: !page.inCall
                icon.name: "call-start"
                text: i18n("Call")
                display: QQC2.AbstractButton.IconOnly
                enabled: page.queryIsNumber
                onClicked: page.calls.dial(query.text)
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }

            QQC2.Button {
                Layout.preferredWidth: Kirigami.Units.gridUnit * 4
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.5
                visible: !page.inCall
                icon.name: "edit-clear-locationbar-rtl"
                text: i18n("Delete")
                display: QQC2.AbstractButton.IconOnly
                onClicked: {
                    if (query.selectedText !== "") {
                        query.remove(query.selectionStart, query.selectionEnd);
                    } else {
                        query.remove(Math.max(0, query.cursorPosition - 1), query.cursorPosition);
                    }
                }
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            }
        }
    }
}
