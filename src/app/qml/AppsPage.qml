/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KD
import io.github.rkrajnc.phonekompanion

Kirigami.ScrollablePage {
    id: page

    required property ScrcpyInterface scrcpy

    title: i18n("Open App")

    // The cached list right away, then the phone's current one
    Component.onCompleted: scrcpy.refreshApps()

    actions: [
        Kirigami.Action {
            icon.name: "view-refresh"
            text: i18n("Refresh")
            enabled: !page.scrcpy.refreshingApps
            onTriggered: page.scrcpy.refreshApps()
        }
    ]

    header: QQC2.Control {
        topPadding: Kirigami.Units.smallSpacing
        bottomPadding: Kirigami.Units.smallSpacing
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing

        background: Rectangle {
            Kirigami.Theme.colorSet: Kirigami.Theme.Window
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            Kirigami.Separator {
                anchors {
                    left: parent.left
                    bottom: parent.bottom
                    right: parent.right
                }
            }
        }

        contentItem: Kirigami.SearchField {
            id: searchField
            autoAccept: false
            focus: true
            onAccepted: {
                if (appList.count === 1) {
                    page.scrcpy.openApp(appList.model[0].package);
                }
            }
            Keys.onDownPressed: event => {
                appList.currentIndex = 0;
                event.accepted = false; // Pass to KeyNavigation.down
            }
            KeyNavigation.down: appList
        }
    }

    ListView {
        id: appList
        Accessible.role: Accessible.List
        model: {
            const text = searchField.text.toLowerCase();
            return page.scrcpy.apps.filter(app => app.name.toLowerCase().includes(text) || app.package.toLowerCase().includes(text));
        }

        delegate: KD.SubtitleDelegate {
            required property var modelData

            width: ListView.view.width
            text: modelData.name
            subtitle: modelData.package
            onClicked: page.scrcpy.openApp(modelData.package)
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: appList.count === 0
            text: {
                if (page.scrcpy.refreshingApps) {
                    return i18n("Reading the apps on the phone…");
                } else if (searchField.text) {
                    return i18n("No app matches");
                }
                return page.scrcpy.appsError || i18n("No apps found");
            }
        }
    }
}
