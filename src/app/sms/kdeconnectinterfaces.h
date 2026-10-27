/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDBusAbstractInterface>
#include <QDBusPendingReply>
#include <QDBusVariant>

/**
 * The SMS conversations of a KDE Connect device (org.kde.kdeconnect.device.conversations), in
 * place of KDE Connect's class of the same name, whose library is not installed. The calls do
 * not start KDE Connect, which has its own autostart.
 */
class DeviceConversationsDbusInterface : public QDBusAbstractInterface
{
    Q_OBJECT

public:
    explicit DeviceConversationsDbusInterface(const QString &deviceId, QObject *parent = nullptr);

    QDBusPendingReply<QVariantList> activeConversations();
    QDBusPendingReply<> requestConversation(qint64 conversationId, int start, int end);
    QDBusPendingReply<> replyToConversation(qint64 conversationId, const QString &message, const QVariantList &attachmentUrls);
    QDBusPendingReply<> sendWithoutConversation(const QVariantList &addresses, const QString &message, const QVariantList &attachmentUrls);
    QDBusPendingReply<> requestAllConversationThreads();
    QDBusPendingReply<> requestAttachmentFile(qint64 partId, const QString &uniqueIdentifier);

Q_SIGNALS:
    // Relayed from D-Bus by QDBusAbstractInterface, by name
    void conversationCreated(const QDBusVariant &msg);
    void conversationRemoved(qint64 conversationID);
    void conversationUpdated(const QDBusVariant &msg);
    void conversationLoaded(qint64 conversationID, quint64 messageCount);
    void attachmentReceived(const QString &filePath, const QString &fileName);

private:
    QDBusPendingCall call(const QString &method, const QVariantList &arguments = {});
};
