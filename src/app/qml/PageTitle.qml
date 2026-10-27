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

/*
 * A page's title with back and forward in front of it, in place of Kirigami's own buttons,
 * which cannot reopen what back closed. Shown on the leftmost visible page, like Kirigami's.
 */
RowLayout {
    id: root

    // Main, for closeLastPage(), goForward(), canReopen and pageTitles
    required property var window
    // Kirigami's page header showing this tells its page; it sits in the column view, not in the page
    readonly property Item page: {
        for (let item = parent; item; item = item.parent) {
            const page = pageOf(item);
            if (page) {
                return page;
            }
        }
        return null;
    }

    // Untyped, as only Kirigami's header has the page property
    function pageOf(header: var): Item {
        return header.page instanceof Kirigami.Page ? header.page : null;
    }
    readonly property Kirigami.PageRow pageStack: window.pageStack
    readonly property bool leading: page !== null && (pageStack.columnView.columnResizeMode === Kirigami.ColumnView.SingleColumn ? page === pageStack.currentItem : page === pageStack.leadingVisibleItem)

    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing

    QQC2.ToolButton {
        visible: root.leading
        enabled: root.pageStack.depth > 1
        icon.name: LayoutMirroring.enabled ? "go-previous-symbolic-rtl" : "go-previous-symbolic"
        text: i18n("Navigate Back")
        display: QQC2.AbstractButton.IconOnly
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        onClicked: root.window.closeLastPage()
    }

    QQC2.ToolButton {
        visible: root.leading
        enabled: root.pageStack.currentIndex < root.pageStack.depth - 1 || root.window.canReopen
        icon.name: LayoutMirroring.enabled ? "go-next-symbolic-rtl" : "go-next-symbolic"
        text: i18n("Navigate Forward")
        display: QQC2.AbstractButton.IconOnly
        QQC2.ToolTip.text: text
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
        onClicked: root.window.goForward()
    }

    // The page's own title, e.g. a heading or a search field
    Loader {
        Layout.fillWidth: (item as Item)?.Layout.fillWidth ?? false
        Layout.minimumWidth: (item as Item)?.Layout.minimumWidth ?? -1
        Layout.preferredWidth: (item as Item)?.Layout.preferredWidth ?? -1
        Layout.maximumWidth: (item as Item)?.Layout.maximumWidth ?? -1
        // Where Kirigami puts a title without buttons in front of it
        Layout.leftMargin: root.leading ? 0 : Kirigami.Units.gridUnit - Kirigami.Units.smallSpacing
        sourceComponent: root.page ? root.window.pageTitles.get(root.page) ?? null : null
    }
}
