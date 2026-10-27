/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.rkrajnc.phonekompanion

Kirigami.ScrollablePage {
    id: page

    required property CallsInterface calls
    readonly property var settings: calls.settings
    // Changes only with the phones, not with every call state, so the combo box is not rebuilt meanwhile
    readonly property string phonesState: JSON.stringify([calls.pairedPhones, calls.connectedPhones])

    title: i18n("Call Settings")

    // The controls are set from the settings, not bound to them, so a click does not break a binding
    function loadSettings() {
        if (Object.keys(settings).length === 0) {
            return;
        }
        useThisComputer.checked = settings.useThisComputer;
        autoConnect.checked = settings.autoConnect;
        phone.currentIndex = Math.max(0, phone.indexOfValue(settings.gatewayAddress.toUpperCase()));
    }

    Component.onCompleted: loadSettings()
    onSettingsChanged: loadSettings()

    Kirigami.FormLayout {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !page.calls.available
            text: i18n("The Bluetooth phones are listed while the Phone Kompanion service is running.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: page.calls.available && !page.calls.telephonyAvailable
            text: i18n("Calls over Bluetooth need PipeWire with telephony support (org.pipewire.Telephony), which is not running.")
        }

        QQC2.ComboBox {
            id: phone
            Kirigami.FormData.label: i18n("Bluetooth phone:")
            textRole: "text"
            valueRole: "address"
            model: {
                const saved = (page.settings.gatewayAddress ?? "").toUpperCase();
                const [phones, connected] = JSON.parse(page.phonesState);
                const items = [{
                    text: i18n("Automatic"),
                    address: ""
                }];
                for (const address of Object.keys(phones).sort()) {
                    const name = phones[address] || address;
                    items.push({
                        text: connected.includes(address) ? i18n("%1 (connected)", name) : name,
                        address: address
                    });
                }
                // A chosen phone that is no longer paired stays selectable, so the page shows what is in use
                if (saved && !(saved in phones)) {
                    items.push({
                        text: saved,
                        address: saved
                    });
                }
                return items;
            }
            onModelChanged: Qt.callLater(() => currentIndex = Math.max(0, indexOfValue((page.settings.gatewayAddress ?? "").toUpperCase())))
            onActivated: page.calls.setSetting("gatewayAddress", currentValue)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Automatic uses the paired Bluetooth phone with the same name as the phone in KDE Connect, or else the only connected one if you have only one phone.")
        }

        QQC2.CheckBox {
            id: useThisComputer
            text: i18n("Use this computer for calls")
            onToggled: page.calls.setSetting("useThisComputer", checked)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("When off, this computer only shows incoming and missed calls, and call audio stays on the phone.")
        }

        QQC2.CheckBox {
            id: autoConnect
            enabled: useThisComputer.checked
            text: i18n("Connect for calls automatically")
            onToggled: page.calls.setSetting("autoConnect", checked)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Connects the phone over Bluetooth when it becomes reachable through KDE Connect.")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Notifications")
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("KDE Connect also shows incoming and missed calls. To see each call once, turn off KDE Connect's \"Incoming call\" and \"Missed call\" notifications.")
        }

        QQC2.Button {
            icon.name: "preferences-desktop-notification-bell"
            text: i18n("Open KDE Connect's Notification Settings")
            onClicked: page.calls.openKdeConnectNotificationSettings()
        }
    }
}
