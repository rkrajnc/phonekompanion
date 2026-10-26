/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import io.github.rkrajnc.phonekompanion

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Phone Kompanion")
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20

    pageStack.initialPage: Kirigami.ScrollablePage {
        title: i18n("Phones")

        ListView {
            id: phoneList

            model: PhonesModel {
                id: phonesModel
            }

            delegate: KD.SubtitleDelegate {
                required property string name
                required property bool reachable

                width: ListView.view.width
                icon.name: "smartphone"
                text: name
                subtitle: reachable ? i18n("Connected") : i18n("Not connected")
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
