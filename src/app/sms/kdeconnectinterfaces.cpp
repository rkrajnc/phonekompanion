/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "kdeconnectinterfaces.h"

#include <QDBusConnection>
#include <QDBusMessage>

DeviceConversationsDbusInterface::DeviceConversationsDbusInterface(const QString &deviceId, QObject *parent)
    : QDBusAbstractInterface(QStringLiteral("org.kde.kdeconnect"),
                             QStringLiteral("/modules/kdeconnect/devices/") + deviceId,
                             "org.kde.kdeconnect.device.conversations",
                             QDBusConnection::sessionBus(),
                             parent)
{
}

QDBusPendingReply<QVariantList> DeviceConversationsDbusInterface::activeConversations()
{
    return call(QStringLiteral("activeConversations"));
}

QDBusPendingReply<> DeviceConversationsDbusInterface::requestConversation(qint64 conversationId, int start, int end)
{
    return call(QStringLiteral("requestConversation"), {conversationId, start, end});
}

QDBusPendingReply<> DeviceConversationsDbusInterface::replyToConversation(qint64 conversationId, const QString &message, const QVariantList &attachmentUrls)
{
    return call(QStringLiteral("replyToConversation"), {conversationId, message, attachmentUrls});
}

QDBusPendingReply<>
DeviceConversationsDbusInterface::sendWithoutConversation(const QVariantList &addresses, const QString &message, const QVariantList &attachmentUrls)
{
    return call(QStringLiteral("sendWithoutConversation"), {addresses, message, attachmentUrls});
}

QDBusPendingReply<> DeviceConversationsDbusInterface::requestAllConversationThreads()
{
    return call(QStringLiteral("requestAllConversationThreads"));
}

QDBusPendingReply<> DeviceConversationsDbusInterface::requestAttachmentFile(qint64 partId, const QString &uniqueIdentifier)
{
    return call(QStringLiteral("requestAttachmentFile"), {partId, uniqueIdentifier});
}

QDBusPendingCall DeviceConversationsDbusInterface::call(const QString &method, const QVariantList &arguments)
{
    QDBusMessage message = QDBusMessage::createMethodCall(service(), path(), interface(), method);
    message.setArguments(arguments);
    message.setAutoStartService(false);
    return connection().asyncCall(message, timeout());
}

#include "moc_kdeconnectinterfaces.cpp"
