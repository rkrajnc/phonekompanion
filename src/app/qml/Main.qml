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

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Phone Kompanion")
    minimumWidth: Kirigami.Units.gridUnit * 20
    minimumHeight: Kirigami.Units.gridUnit * 20

    // The start opened a phone page, or a phone page was opened before; only once
    property bool phoneOpened: false
    function openPhone(phoneId: string, phoneName: string): PhonePage {
        phoneOpened = true;
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

    // subpage is "apps", "dialer", "messages" or empty; number one for the dialer, or with text for a
    // conversation in Messages; without a phone id, the phone suiting the subpage
    function openPhoneById(phoneId: string, subpage: string, number: string, text: string) {
        const rows = {
            messages: () => phonesModel.rowForMessages(),
            apps: () => phonesModel.rowForReachable(),
            dialer: () => phonesModel.rowForCalls()
        };
        const row = phoneId ? phonesModel.indexOf(phoneId) : (rows[subpage] ?? rows.dialer)();
        if (row < 0) {
            // Dropped once KDE Connect's phones were read, as it is then not paired
            pendingPhone = phonesModel.loaded && phonesModel.kdeConnectRunning ? null : {
                phoneId: phoneId,
                subpage: subpage,
                number: number,
                text: text
            };
            return;
        }
        pendingPhone = null;
        const index = phonesModel.index(row, 0);
        const page = openPhone(phonesModel.data(index, PhonesModel.PhoneIdRole), phonesModel.data(index));
        if (subpage === "apps") {
            page.openApps();
        } else if (subpage === "dialer") {
            page.openDialer().setNumber(number);
        } else if (subpage === "messages") {
            page.openMessages(number, text);
        }
    }

    // Pages back closed, newest last, for forward to reopen one after the other like a browser:
    // the depth each reopens at, how, and whether that is still possible. They reopen through
    // the page now open before them, as the one they were opened from may be gone.
    property var closedPages: []
    // Set while forward reopens a page; any other page opened starts a new history
    property bool reopening: false
    // For canReopen, as phonesModel.indexOf() tells the binding nothing
    property int phonesRevision: 0
    readonly property bool canReopen: {
        phonesRevision;
        return reopenable() !== null;
    }

    // The entry forward would reopen now, or null
    function reopenable(): var {
        const closed = closedPages[closedPages.length - 1];
        return closed !== undefined && closed.depth === pageStack.depth && closed.valid() ? closed : null;
    }

    // Back closes the last page, rather than only scrolling or moving the focus left, and
    // remembers it for forward: a phone page, the pages it opened, and conversations.
    function closeLastPage() {
        const depth = pageStack.depth;
        if (depth < 2) {
            return;
        }
        const closing = pageStack.get(depth - 1);
        const previous = pageStack.get(depth - 2);
        const opener = () => pageStack.get(depth - 2);
        let closed = null;
        if (closing instanceof PhonePage) {
            const phoneId = closing.phoneId;
            const phoneName = closing.phoneName;
            // Not after the phone was unpaired
            closed = {
                reopen: () => openPhone(phoneId, phoneName),
                valid: () => phonesModel.indexOf(phoneId) >= 0
            };
        } else if (typeof previous.openConversation === "function") {
            // A conversation, from KDE Connect's message list
            const conversation = {
                addresses: closing.addresses,
                conversationId: closing.conversationId,
                isMultitarget: closing.isMultitarget
            };
            closed = {
                reopen: () => opener().openConversation(conversation),
                valid: () => typeof opener()?.openConversation === "function"
            };
        } else if (previous instanceof PhonePage && previous.subpage) {
            const phoneId = previous.phoneId;
            const subpage = previous.subpage;
            // E.g. not Messages while the phone is away, and not for another phone
            closed = {
                reopen: () => opener().openSubpage(subpage),
                valid: () => opener() instanceof PhonePage && opener().phoneId === phoneId && opener().canOpen(subpage)
            };
        }
        pageStack.pop();
        if (closed) {
            closed.depth = depth - 1;
            closedPages = closedPages.concat([closed]);
        } else {
            closedPages = [];
        }
    }

    // Reopens what back closed last, else moves to the page right of the current one. With one
    // column, a page to the right is hidden, so that one comes first; with several, it is already
    // in view, and clicking a title further left only made that page current.
    function goForward() {
        const singleColumn = pageStack.columnView.columnResizeMode === Kirigami.ColumnView.SingleColumn;
        const closed = reopenable();
        if (closed && !(singleColumn && pageStack.currentIndex < pageStack.depth - 1)) {
            closedPages = closedPages.slice(0, -1);
            // push() inserts after the current page
            pageStack.currentIndex = pageStack.depth - 1;
            reopening = true;
            try {
                closed.reopen();
            } finally {
                reopening = false;
            }
        } else if (pageStack.currentIndex < pageStack.depth - 1) {
            pageStack.goForward();
        }
    }

    // Back from the keyboard
    Connections {
        target: root.pageStack.currentItem
        function onBackRequested(event) {
            event.accepted = true;
            root.closeLastPage();
        }
    }

    // The mouse's back and forward buttons, which Kirigami leaves alone. Forward from the
    // keyboard stays Kirigami's (moving the focus right), as a second shortcut on its key would
    // make both ambiguous.
    MouseArea {
        anchors.fill: parent
        z: 1000
        acceptedButtons: Qt.BackButton | Qt.ForwardButton
        onClicked: mouse => {
            if (mouse.button === Qt.ForwardButton) {
                root.goForward();
            } else if (root.pageStack.layers.depth > 1) {
                // E.g. an attachment shown over its conversation
                root.pageStack.layers.pop();
            } else {
                root.closeLastPage();
            }
        }
    }

    // Back and forward are in each page's title instead, always there and greyed out when there
    // is nowhere to go; Kirigami's own in other styles, e.g. on a tablet
    readonly property bool ownNavigation: pageStack.globalToolBar.actualStyle === Kirigami.ApplicationHeaderStyle.ToolBar
    pageStack.globalToolBar.showNavigationButtons: ownNavigation ? Kirigami.ApplicationHeaderStyle.NoNavigationButtons : (Kirigami.ApplicationHeaderStyle.ShowBackButton | Kirigami.ApplicationHeaderStyle.ShowForwardButton)

    // Each page's own title delegate, shown after the buttons
    property var pageTitles: new Map()

    // Only for the desktop's toolbar style, whose page headers PageTitle knows
    function addPageTitle(page: Kirigami.Page) {
        if (ownNavigation && !pageTitles.has(page)) {
            pageTitles.set(page, page.titleDelegate);
            page.titleDelegate = pageTitle;
        }
    }

    Component {
        id: pageTitle
        PageTitle {
            window: root
        }
    }

    // pageInserted comes for pushed pages too
    Connections {
        target: root.pageStack
        function onPageInserted(position, page) {
            root.addPageTitle(page);
            if (!root.reopening) {
                root.closedPages = [];
            }
        }
        function onPageRemoved(page) {
            root.pageTitles.delete(page);
        }
    }

    Component.onCompleted: addPageTitle(pageStack.initialPage)

    Component {
        id: phonePage
        PhonePage {}
    }

    pageStack.initialPage: Kirigami.ScrollablePage {
        id: phonesPage

        title: i18n("Phones")

        ListView {
            id: phoneList

            model: PhonesModel {
                id: phonesModel
                // Close the page of a phone that was unpaired, or of all phones when the service stops
                onModelReset: {
                    root.phonesRevision++;
                    if (root.pendingPhone && loaded) {
                        root.openPhoneById(root.pendingPhone.phoneId, root.pendingPhone.subpage, root.pendingPhone.number, root.pendingPhone.text);
                    }
                    // At the start, the phone KDE Connect reaches, else the first one, as after going back;
                    // not later, e.g. when a phone is paired while the window is open
                    if (!root.phoneOpened && !root.pendingPhone && loaded && rowCount() > 0) {
                        const row = rowForReachable();
                        root.openPhone(data(index(row, 0), PhonesModel.PhoneIdRole), data(index(row, 0)));
                    }
                    if (loaded) {
                        root.phoneOpened = true;
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
