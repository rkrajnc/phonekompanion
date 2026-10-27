/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import io.github.rkrajnc.phonekompanion

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Phone Kompanion")
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20

    function openPhone(phoneId: string, phoneName: string): PhonePage {
        // Keeps its apps or settings page, e.g. while a QR code is shown
        const page = pageStack.get(1);
        if (page?.phoneId === phoneId) {
            return page;
        }
        pageStack.pop(pageStack.initialPage);
        return pageStack.push(phonePage, {
            phoneId: phoneId,
            phoneName: phoneName
        });
    }

    // Waits for the daemon's phones, e.g. right after the window started
    property var pendingPhone: null

    function openPhoneById(phoneId: string, apps: bool) {
        const row = phonesModel.indexOf(phoneId);
        if (row < 0) {
            // An unknown phone is dropped once the phones were read
            pendingPhone = phonesModel.loaded ? null : {
                phoneId: phoneId,
                apps: apps
            };
            return;
        }
        pendingPhone = null;
        const page = openPhone(phoneId, phonesModel.data(phonesModel.index(row, 0)));
        if (apps) {
            page.openApps();
        }
    }

    Component {
        id: phonePage
        PhonePage {}
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        title: i18n("Phones")

        ListView {
            id: phoneList

            model: PhonesModel {
                id: phonesModel
                // Close the page of a phone that was unpaired, or of all phones when the service stops
                onModelReset: {
                    if (root.pendingPhone && loaded) {
                        root.openPhoneById(root.pendingPhone.phoneId, root.pendingPhone.apps);
                    }
                    const page = root.pageStack.get(1);
                    if (page && indexOf(page.phoneId) < 0) {
                        root.pageStack.pop(root.pageStack.initialPage);
                    }
                }
            }

            delegate: KD.SubtitleDelegate {
                required property string phoneId
                required property string name
                required property bool reachable

                width: ListView.view.width
                icon.name: "smartphone"
                text: name
                subtitle: reachable ? i18n("Connected") : i18n("Not connected")
                highlighted: root.pageStack.get(1)?.phoneId === phoneId
                onClicked: root.openPhone(phoneId, name)
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: phoneList.count === 0
                icon.name: "smartphone"
                text: {
                    if (!phonesModel.serviceRunning) {
                        return i18n("The Phone Kompanion service is not running");
                    }
                    return phonesModel.kdeConnectRunning ? i18n("No phones paired in KDE Connect") : i18n("KDE Connect is not running");
                }
                explanation: phonesModel.serviceRunning && phonesModel.kdeConnectRunning ? i18n("Pair your phone in KDE Connect and it shows up here.") : ""
                helpfulAction: Kirigami.Action {
                    icon.name: phonesModel.serviceRunning ? "kdeconnect" : "media-playback-start"
                    text: phonesModel.serviceRunning ? i18n("Open KDE Connect") : i18n("Start the Service")
                    onTriggered: phonesModel.serviceRunning ? phonesModel.openKdeConnect() : phonesModel.startService()
                }
            }
        }
    }
}
