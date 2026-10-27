/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "callsinterface.h"

#include <QDBusArgument>
#include <QDBusMessage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>

#include "dbusnames.h"

CallsInterface::CallsInterface(QObject *parent)
    : PhoneFeature(QStringLiteral("calls"), PHONEKOMPANION_CALLS_INTERFACE, parent)
{
}

bool CallsInterface::gatewayConnected() const
{
    return status(QStringLiteral("gatewayConnected")).toBool();
}

bool CallsInterface::telephonyAvailable() const
{
    return status(QStringLiteral("telephonyAvailable")).toBool();
}

QVariantMap CallsInterface::pairedPhones() const
{
    return qdbus_cast<QVariantMap>(status(QStringLiteral("pairedPhones")));
}

QStringList CallsInterface::connectedPhones() const
{
    return status(QStringLiteral("connectedPhones")).toStringList();
}

QString CallsInterface::callState() const
{
    return status(QStringLiteral("callState")).toString();
}

QString CallsInterface::callName() const
{
    return status(QStringLiteral("callName")).toString();
}

QString CallsInterface::callNumber() const
{
    return status(QStringLiteral("callNumber")).toString();
}

bool CallsInterface::audioOnPc() const
{
    return status(QStringLiteral("audioOnPc")).toBool();
}

bool CallsInterface::microphoneMuted() const
{
    return status(QStringLiteral("microphoneMuted")).toBool();
}

bool CallsInterface::microphoneMuteSupported() const
{
    return status(QStringLiteral("microphoneMuteSupported")).toBool();
}

QVariantList CallsInterface::contacts() const
{
    return m_contacts;
}

QString CallsInterface::contactsQuery() const
{
    return m_contactsQuery;
}

void CallsInterface::searchContacts(const QString &query)
{
    const quint64 search = ++m_search;
    if (query.trimmed().isEmpty()) {
        m_contacts.clear();
        m_contactsQuery = query;
        Q_EMIT contactsChanged();
        return;
    }
    call(QStringLiteral("searchContacts"), {query}, [this, search, query](const QDBusMessage &reply) {
        if (search != m_search) {
            return;
        }
        m_contactsQuery = query;
        m_contacts = reply.type() == QDBusMessage::ReplyMessage
            ? QJsonDocument::fromJson(reply.arguments().value(0).toString().toUtf8()).array().toVariantList()
            : QVariantList();
        Q_EMIT contactsChanged();
    });
}

void CallsInterface::dial(const QString &numberOrName)
{
    callAction(QStringLiteral("dial"), {numberOrName});
}

void CallsInterface::answer()
{
    callAction(QStringLiteral("answer"));
}

void CallsInterface::hangUp()
{
    callAction(QStringLiteral("hangUp"));
}

void CallsInterface::moveAudioToPc()
{
    callAction(QStringLiteral("moveAudioToPc"));
}

void CallsInterface::sendTones(const QString &tones)
{
    callAction(QStringLiteral("sendTones"), {tones});
}

void CallsInterface::setMicrophoneMuted(bool muted)
{
    callAction(QStringLiteral("setMicrophoneMuted"), {muted});
}

void CallsInterface::openKdeConnectNotificationSettings()
{
    if (!QProcess::startDetached(QStringLiteral("kcmshell6"), {QStringLiteral("kcm_notifications"), QStringLiteral("--args=--notifyrc=kdeconnect")})) {
        qWarning() << "Could not start kcmshell6";
    }
}

void CallsInterface::reset()
{
    ++m_search;
    m_contacts.clear();
    m_contactsQuery.clear();
    PhoneFeature::reset();
    Q_EMIT contactsChanged();
}

QList<PhoneFeature::SignalSlot> CallsInterface::signalSlots() const
{
    // The call and the gateway are part of the status
    return {
        {"callChanged", SLOT(readStatus())},
        {"gatewayConnectedChanged", SLOT(readStatus())},
    };
}

#include "moc_callsinterface.cpp"
