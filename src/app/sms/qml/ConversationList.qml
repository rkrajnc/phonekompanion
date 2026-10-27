/**
 * SPDX-FileCopyrightText: 2018 Aleix Pol Gonzalez <aleixpol@kde.org>
 * SPDX-FileCopyrightText: 2018 Nicolas Fella <nicolas.fella@gmx.de>
 * SPDX-FileCopyrightText: 2018 Simon Redman <simon@ergotech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KirigamiDelegates
import org.kde.kirigami.platform as KirigamiPlatform
import io.github.rkrajnc.phonekompanion.sms

Kirigami.ScrollablePage
{
    id: page

    ToolTip {
        visible: !deviceConnected
        timeout: -1
        text: "⚠️ " + i18nd("kdeconnect-sms", "No devices available") + " ⚠️"

        MouseArea {
            // Detect mouseover and show another tooltip with more information
            anchors.fill: parent
            hoverEnabled: true

            ToolTip.visible: containsMouse
            ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            ToolTip.text: i18nd("kdeconnect-sms", "No new messages can be sent or received, but you can browse cached content")
        }
    }

    ColumnLayout {
        id: loadingMessage
        visible: deviceConnected && view.count === 0 && currentSearchText.length == 0
        anchors.centerIn: parent

        BusyIndicator {
            visible: loadingMessage.visible
            running: loadingMessage.visible
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
        }

        Label {
            text: i18nd("kdeconnect-sms", "Loading conversations from device. If this takes a long time, please wake up your device and then click Refresh.")
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.preferredWidth: page.width / 2
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Label {
            text: i18nd("kdeconnect-sms", "Tip: If you plug in your device, it should not go into doze mode and should load quickly.")
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            Layout.preferredWidth: page.width / 2
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        Button {
            text: i18nd("kdeconnect-sms", "Refresh")
            icon.name: "view-refresh"
            Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
            onClicked: {
                conversationListModel.refresh();
            }
        }
    }

    required property string deviceId
    property string initialMessage

    // Phone Kompanion: also for reopening a conversation, with forward after back closed it
    function openConversation(properties) {
        return applicationWindow().pageStack.push(chatView, properties);
    }

    // Phone Kompanion: when the phone is back, as KDE Connect does not send the list by itself
    function refresh() {
        conversationListModel.refresh();
    }

    // Phone Kompanion: an sms: link, {address, message}, set by the phone page when it opens the
    // list: the one-to-one conversation with the address and the message ready to send. An
    // existing one opens as soon as it is there, a new one once the conversations stop coming;
    // nothing if a conversation was opened meanwhile.
    property var wantedConversation: null

    Component.onCompleted: findWantedConversation(false)

    function findWantedConversation(create) {
        const wanted = wantedConversation;
        if (!wanted) {
            return;
        }
        // A page after this list, e.g. a conversation the user opened; -1 while the list is pushed
        const index = Kirigami.ColumnView.index;
        if (index >= 0 && index < applicationWindow().pageStack.depth - 1) {
            wantedConversation = null;
            return;
        }
        let conversation = conversationListModel.conversationForAddress(wanted.address);
        if (conversation.conversationId === undefined) {
            if (!create) {
                conversationsSettled.restart();
                return;
            } else if (!SmsHelper.isAddressValid(wanted.address)) {
                wantedConversation = null;
                return;
            }
            conversationListModel.createConversationForAddress(wanted.address);
            conversation = conversationListModel.conversationForAddress(wanted.address);
        }
        wantedConversation = null;
        openConversation({
            addresses: conversation.addresses,
            conversationId: conversation.conversationId,
            isMultitarget: false,
            initialMessage: wanted.message
        });
    }

    // The conversations come one by one; a new conversation once they stop for a moment, or when
    // none came for a while
    Timer {
        id: conversationsSettled
        interval: view.count > 0 ? 2000 : 10000
        onTriggered: page.findWantedConversation(true)
    }

    Connections {
        target: view
        function onCountChanged() {
            page.findWantedConversation(false);
        }
    }

    property string currentSearchText
    property alias conversationListModel: conversationListModel
    property bool deviceConnected

    header: Kirigami.InlineMessage {
        Layout.fillWidth: true
        position: Kirigami.InlineMessage.Header
        visible: page.initialMessage.length > 0
        text: i18nd("kdeconnect-sms", "Choose recipient")

        actions: [
          Kirigami.Action {
              icon.name: "dialog-cancel"
              text: i18nd("kdeconnect-sms", "Cancel")
              onTriggered: initialMessage = ""
            }
        ]
    }

    titleDelegate: RowLayout {
        spacing: Kirigami.Units.mediumSpacing

        Layout.fillWidth: true

        Keys.forwardTo: [filter]

        Kirigami.SearchField {
            /**
             * Used as the filter of the list of messages
             */
            id: filter
            Layout.fillWidth: true
            placeholderText: i18nd("kdeconnect-sms", "Search or start a conversation")
            enabled: deviceConnected
            onTextChanged: {
                currentSearchText = filter.text;
                if (filter.text !== "") {
                    view.model.setConversationsFilterRole(ConversationListModel.AddressesRole)
                } else {
                    view.model.setConversationsFilterRole(ConversationListModel.ConversationIdRole)
                }
                view.model.setFilterFixedString(SmsHelper.canonicalizePhoneNumber(filter.text))

                view.currentIndex = 0
                filter.forceActiveFocus();
            }

            Keys.onReturnPressed: event => {
                event.accepted = true
                view.currentItem.startChat()
            }
            Keys.onEscapePressed: event => {
                event.accepted = filter.text !== ""
                filter.text = ""
            }
            Shortcut {
                sequence: "Ctrl+F"
                onActivated: filter.forceActiveFocus()
            }
        }

        Button {
            id: newButton
            icon.name: "list-add"
            enabled: SmsHelper.isAddressValid(filter.text) && deviceConnected

            ToolTip.visible: hovered
            ToolTip.delay: Qt.styleHints.mousePressAndHoldInterval
            ToolTip.text: i18nd("kdeconnect-sms", "Start new conversation")

            onClicked: {
                // We have to disable the filter temporarily in order to avoid getting key inputs accidentally while processing the request
                filter.enabled = false

                // If the address entered by the user already exists then ignore adding new contact
                if (!view.model.doesAddressExists(filter.text) && SmsHelper.isAddressValid(filter.text)) {
                    conversationListModel.createConversationForAddress(filter.text)
                    view.currentIndex = 0
                }
                view.currentItem.startChat()
                filter.enabled = true
            }

            Shortcut {
                sequence: "Ctrl+N"
                onActivated: newButton.clicked()
            }
        }
    }

    Component {
        id: chatView
        ConversationDisplay {
            deviceId: page.deviceId
            deviceConnected: page.deviceConnected
        }
    }

    ListView {
        id: view
        model: QSortFilterProxyModel {
            sortOrder: Qt.DescendingOrder
            filterCaseSensitivity: Qt.CaseInsensitive
            sourceModel: ConversationListModel {
                id: conversationListModel
                deviceId: page.deviceId
            }
        }

        Component.onCompleted: {
            currentIndex = -1
            focus = true
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)
            visible: deviceConnected && view.count === 0 && currentSearchText.length !== 0
            icon.name: "search"
            text: i18ndc("kdeconnect-sms", "Placeholder message text when no messages are found", "No Matches")
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - (Kirigami.Units.largeSpacing * 4)

            visible: view.count === 0 && currentSearchText.length === 0

            icon.name: "view-conversation-balloon"
            text: i18ndc("kdeconnect-sms", "@info:placeholder", "No Conversations")
        }

        Keys.forwardTo: [headerItem]

        delegate: ItemDelegate {
            id: listItem
            text: displayNames
            width: view.width

            required property string displayNames
            required property string toolTip
            required property var decoration
            required property var attachmentPreview
            required property int index
            required property var addresses
            required property bool isMultitarget
            required property int conversationId

            // Keep the currently-open chat highlighted even if this element is not focused
            highlighted: ListView.isCurrentItem

            function startChat() {
                view.currentItem.forceActiveFocus();
                page.openConversation({
                                          addresses: listItem.addresses,
                                          conversationId: listItem.conversationId,
                                          isMultitarget: listItem.isMultitarget,
                                          initialMessage: page.initialMessage})
                initialMessage = ""
            }

            onClicked: {
                view.currentIndex = index
                startChat();
            }

            // Note: Width calcs to account for scrollbar coming and going
            contentItem: RowLayout {
                id: contentRow
                spacing: Kirigami.Units.smallSpacing
                implicitWidth: view.width - Kirigami.Units.largeSpacing

                Kirigami.Icon {
                    id: contactIcon
                    source: listItem.decoration
                }

                // Set width here to force elide and account for scrollbar
                KirigamiDelegates.TitleSubtitle {
                    title: listItem.text
                    subtitle: listItem.toolTip
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                Kirigami.ShadowedImage {
                    id: thumbnailItem
                    source: listItem.attachmentPreview === undefined ? "" : `image://thumbnailsProvider/${listItem.attachmentPreview}`
                    visible: listItem.attachmentPreview !== undefined
                    width: KirigamiPlatform.Units.iconSizes.medium
                    height: KirigamiPlatform.Units.iconSizes.medium
                    radius: thumbnailItem.width / 6
                }
            }
        }
    }
}
