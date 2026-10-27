/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "phonefeature.h"

/**
 * A phone's Bluetooth calls, /phones/<id>/calls of phonekompaniond
 */
class CallsInterface : public PhoneFeature
{
    Q_OBJECT
    QML_ELEMENT
    // Calls can be made and answered here: the phone is connected over Bluetooth and calls are on
    Q_PROPERTY(bool gatewayConnected READ gatewayConnected NOTIFY statusChanged)
    // PipeWire's telephony API is there
    Q_PROPERTY(bool telephonyAvailable READ telephonyAvailable NOTIFY statusChanged)
    // Bluetooth address -> name of the paired phones
    Q_PROPERTY(QVariantMap pairedPhones READ pairedPhones NOTIFY statusChanged)
    Q_PROPERTY(QStringList connectedPhones READ connectedPhones NOTIFY statusChanged)
    // The call to show: "incoming", "waiting", "dialing", "alerting", "active", "held", or empty
    Q_PROPERTY(QString callState READ callState NOTIFY statusChanged)
    Q_PROPERTY(QString callName READ callName NOTIFY statusChanged)
    Q_PROPERTY(QString callNumber READ callNumber NOTIFY statusChanged)
    Q_PROPERTY(bool audioOnPc READ audioOnPc NOTIFY statusChanged)
    Q_PROPERTY(bool microphoneMuted READ microphoneMuted NOTIFY statusChanged)
    Q_PROPERTY(bool microphoneMuteSupported READ microphoneMuteSupported NOTIFY statusChanged)
    // {name, number} objects from the last searchContacts() that answered
    Q_PROPERTY(QVariantList contacts READ contacts NOTIFY contactsChanged)
    // The query contacts are for; while a newer search runs, not the last one sent
    Q_PROPERTY(QString contactsQuery READ contactsQuery NOTIFY contactsChanged)

public:
    explicit CallsInterface(QObject *parent = nullptr);

    bool gatewayConnected() const;
    bool telephonyAvailable() const;
    QVariantMap pairedPhones() const;
    QStringList connectedPhones() const;
    QString callState() const;
    QString callName() const;
    QString callNumber() const;
    bool audioOnPc() const;
    bool microphoneMuted() const;
    bool microphoneMuteSupported() const;
    QVariantList contacts() const;
    QString contactsQuery() const;

    /**
     * Looks for contacts whose name contains query; answers with contactsChanged
     */
    Q_INVOKABLE void searchContacts(const QString &query);
    Q_INVOKABLE void dial(const QString &numberOrName);
    Q_INVOKABLE void answer();
    Q_INVOKABLE void hangUp();
    Q_INVOKABLE void moveAudioToPc();
    Q_INVOKABLE void sendTones(const QString &tones);
    Q_INVOKABLE void setMicrophoneMuted(bool muted);
    /**
     * For turning off KDE Connect's own call notifications, so each call shows once
     */
    Q_INVOKABLE void openKdeConnectNotificationSettings();

Q_SIGNALS:
    void contactsChanged();

protected:
    void reset() override;
    QList<SignalSlot> signalSlots() const override;

private:
    QVariantList m_contacts;
    QString m_contactsQuery;
    // Answers to an older search are dropped
    quint64 m_search = 0;
};
