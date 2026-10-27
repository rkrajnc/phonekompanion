/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.prison as Prison
import io.github.rkrajnc.phonekompanion

Kirigami.ScrollablePage {
    id: page

    required property ScrcpyInterface scrcpy
    readonly property var settings: scrcpy.settings
    readonly property bool canPair: scrcpy.available && !scrcpy.pairing && !scrcpy.missingTools.includes("adb")

    title: i18n("Mirroring Settings")

    // The controls are set from the settings, not bound to them, so a click does not break a binding
    function loadSettings() {
        if (Object.keys(settings).length === 0) {
            return;
        }
        audio.checked = settings.audio;
        turnScreenOff.checked = settings.turnScreenOff;
        keyboard.currentIndex = Math.max(0, keyboard.indexOfValue(settings.keyboard));
        if (!extraOptions.activeFocus) {
            extraOptions.text = settings.extraOptions;
        }
        appDisplay.currentIndex = Math.max(0, appDisplay.indexOfValue(settings.appDisplay));
        appDensity.value = settings.appDensity;
        camera.currentIndex = Math.max(0, camera.indexOfValue(settings.camera));
        cameraRotation.currentIndex = Math.max(0, cameraRotation.indexOfValue(settings.cameraRotation));
        adbDevice.currentIndex = Math.max(0, adbDevice.indexOfValue(settings.adbSerial));
    }

    Component.onCompleted: loadSettings()
    onSettingsChanged: loadSettings()

    // Also while startQrPairing() has not answered yet; the phone page and its scrcpy may be gone first
    Component.onDestruction: {
        if (scrcpy?.pairing) {
            scrcpy.stopQrPairing();
        }
    }

    Connections {
        target: page.scrcpy
        function onPairingFinished(success, message) {
            if (success) {
                pairCode.text = "";
            }
            pairResult.type = success ? Kirigami.MessageType.Positive : Kirigami.MessageType.Error;
            pairResult.text = message;
            pairResult.visible = true;
        }
    }

    Kirigami.FormLayout {
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !page.scrcpy.available
            text: i18n("The status is shown while the Phone Kompanion service is running.")
        }

        // Missing tools or no network; Wireless debugging below, with more help
        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Warning
            visible: page.scrcpy.missingTools.length > 0 || (page.scrcpy.available && !page.scrcpy.networkReachable)
            text: page.scrcpy.unavailableReason
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: page.scrcpy.available && page.scrcpy.networkReachable && page.scrcpy.adbSerial === "" && !page.scrcpy.missingTools.includes("adb")
            text: adbDevice.currentValue ? i18n("The chosen adb device is not connected. Turn on Wireless debugging on the phone, or choose Automatic.") : i18n("Turn on Wireless debugging on the phone, in Developer options. For one-tap access, add its tile under Developer options, Quick settings developer tiles. If this computer is not paired with the phone yet, pair it below.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Positive
            visible: page.scrcpy.adbSerial !== ""
            text: i18n("Connected over adb as %1.", page.scrcpy.adbSerial)
        }

        QQC2.ComboBox {
            id: adbDevice
            Kirigami.FormData.label: i18n("adb device:")
            textRole: "text"
            valueRole: "serial"
            model: {
                const saved = page.settings.adbSerial ?? "";
                const devices = page.scrcpy.adbDevices;
                const items = [{
                    text: i18n("Automatic"),
                    serial: ""
                }];
                for (const serial of devices) {
                    items.push({
                        text: serial,
                        serial: serial
                    });
                }
                // A chosen device that is not connected stays selectable, so the page shows what is in use
                if (saved && !devices.includes(saved)) {
                    items.push({
                        text: saved,
                        serial: saved
                    });
                }
                return items;
            }
            onModelChanged: Qt.callLater(() => currentIndex = Math.max(0, indexOfValue(page.settings.adbSerial ?? "")))
            onActivated: page.scrcpy.setSetting("adbSerial", currentValue)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Automatic uses the adb device with the phone's network address.")
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Pairing")
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Needed once. On the phone, open Developer options, Wireless debugging, Pair device with QR code, and scan the code shown here.")
        }

        QQC2.Button {
            visible: !page.scrcpy.qrContent
            enabled: page.canPair
            text: i18n("Show QR Code")
            onClicked: {
                pairResult.visible = false;
                page.scrcpy.startQrPairing();
            }
        }

        // The form lays out the barcode at its minimum width, so it gets a fixed size here
        Item {
            visible: page.scrcpy.qrContent !== ""
            implicitWidth: Kirigami.Units.gridUnit * 12
            implicitHeight: implicitWidth

            Prison.Barcode {
                anchors.fill: parent
                barcodeType: Prison.Barcode.QRCode
                content: page.scrcpy.qrContent
                // Dark on light also in dark color schemes, for the phone's camera
                foregroundColor: "black"
                backgroundColor: "white"
            }
        }

        RowLayout {
            visible: page.scrcpy.qrContent !== ""
            spacing: Kirigami.Units.smallSpacing

            QQC2.BusyIndicator {
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                running: parent.visible
            }

            QQC2.Label {
                text: i18n("Waiting for the phone…")
            }

            QQC2.Button {
                text: i18n("Cancel")
                onClicked: page.scrcpy.stopQrPairing()
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Or use Pair device with pairing code on the phone, and enter what it shows.")
        }

        QQC2.TextField {
            id: pairAddress
            Kirigami.FormData.label: i18n("IP address and port:")
            placeholderText: "192.168.1.10:37123"
        }

        QQC2.TextField {
            id: pairCode
            Kirigami.FormData.label: i18n("Pairing code:")
            inputMethodHints: Qt.ImhDigitsOnly
            maximumLength: 6
        }

        QQC2.Button {
            enabled: page.canPair
            text: i18n("Pair")
            onClicked: {
                pairResult.visible = false;
                page.scrcpy.pair(pairAddress.text, pairCode.text);
            }
        }

        Kirigami.InlineMessage {
            id: pairResult
            Layout.fillWidth: true
            visible: false
        }

        Kirigami.Separator {
            Kirigami.FormData.isSection: true
            Kirigami.FormData.label: i18n("Mirroring")
        }

        QQC2.CheckBox {
            id: audio
            text: i18n("Play the phone's sound on this computer")
            onToggled: page.scrcpy.setSetting("audio", checked)
        }

        QQC2.CheckBox {
            id: turnScreenOff
            text: i18n("Turn the phone's screen off while mirroring")
            onToggled: page.scrcpy.setSetting("turnScreenOff", checked)
        }

        QQC2.ComboBox {
            id: keyboard
            Kirigami.FormData.label: i18n("Keyboard:")
            textRole: "text"
            valueRole: "mode"
            model: [
                {
                    text: i18n("Simulated key presses"),
                    mode: ""
                },
                {
                    text: i18n("Physical keyboard, with the phone's keyboard layout"),
                    mode: "uhid"
                }
            ]
            onActivated: page.scrcpy.setSetting("keyboard", currentValue)
        }

        QQC2.ComboBox {
            id: camera
            Kirigami.FormData.label: i18n("Phone camera:")
            textRole: "text"
            valueRole: "facing"
            model: [
                {
                    text: i18nc("@item:inlistbox camera facing", "Back"),
                    facing: "back"
                },
                {
                    text: i18nc("@item:inlistbox camera facing", "Front"),
                    facing: "front"
                }
            ]
            onActivated: page.scrcpy.setSetting("camera", currentValue)
        }

        QQC2.ComboBox {
            id: cameraRotation
            Kirigami.FormData.label: i18n("Camera rotation:")
            textRole: "text"
            valueRole: "degrees"
            model: [0, 90, 180, 270].map(degrees => ({
                        text: i18nc("rotation angle", "%1°", degrees),
                        degrees: degrees
                    }))
            onActivated: page.scrcpy.setSetting("cameraRotation", currentValue)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Turns the camera picture upright. Apps showing the webcam need to open it again after a change.")
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Webcam:")
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: {
                if (!page.scrcpy.available) {
                    return "";
                } else if (page.scrcpy.webcamDevice) {
                    return i18n("Other apps can use the phone camera as \"%1\" (%2).", page.scrcpy.webcamName, page.scrcpy.webcamDevice);
                }
                return i18n("To use the phone camera as a webcam, install and load the v4l2loopback kernel module.");
            }
        }

        QQC2.ComboBox {
            id: appDisplay
            Kirigami.FormData.label: i18n("App windows:")
            textRole: "text"
            valueRole: "display"
            model: [
                {
                    text: i18n("Like the phone screen"),
                    display: "phone"
                },
                {
                    text: i18n("Resizable, portrait"),
                    display: "portrait"
                },
                {
                    text: i18n("Resizable, landscape"),
                    display: "landscape"
                }
            ]
            onActivated: page.scrcpy.setSetting("appDisplay", currentValue)
        }

        QQC2.SpinBox {
            id: appDensity
            enabled: appDisplay.currentValue !== "phone"
            Kirigami.FormData.label: i18n("App density (dpi):")
            from: 120
            to: 640
            stepSize: 20
            onValueModified: page.scrcpy.setSetting("appDensity", value)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Apps open on a virtual display of the phone. Like the phone screen, apps look as on the phone and the window scales them. Resizable, the display follows the window's size, and a higher density makes text and buttons larger.")
        }

        QQC2.TextField {
            id: extraOptions
            Kirigami.FormData.label: i18n("Extra scrcpy options:")
            placeholderText: "--max-size=1920 --video-bit-rate=8M"
            onEditingFinished: page.scrcpy.setSetting("extraOptions", text)
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: i18n("Used for the screen and app windows, not for the camera.")
        }
    }
}
