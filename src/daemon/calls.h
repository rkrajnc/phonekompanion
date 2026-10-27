/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KConfigGroup>
#include <KNotification>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QVariantMap>

#include "bluetooth.h"

class Phone;
class QDBusMessage;

/**
 * The phone's calls over Bluetooth (HFP, through PipeWire's telephony API), at /phones/<id>/calls
 */
class Calls : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.rkrajnc.phonekompanion.calls")
    Q_PROPERTY(bool gatewayConnected READ gatewayConnected NOTIFY gatewayConnectedChanged)
    Q_PROPERTY(bool telephonyAvailable READ telephonyAvailable NOTIFY statusChanged)
    Q_PROPERTY(QVariantMap pairedPhones READ pairedPhones NOTIFY statusChanged)
    Q_PROPERTY(QStringList connectedPhones READ connectedPhones NOTIFY statusChanged)
    // The call shown in the dialer: a ringing one first, then an outgoing one, then an active one
    Q_PROPERTY(QString callState READ callState NOTIFY callChanged)
    Q_PROPERTY(QString callName READ callName NOTIFY callChanged)
    Q_PROPERTY(QString callNumber READ callNumber NOTIFY callChanged)
    Q_PROPERTY(bool audioOnPc READ audioOnPc NOTIFY callChanged)
    Q_PROPERTY(bool microphoneMuted READ microphoneMuted NOTIFY callChanged)
    Q_PROPERTY(bool microphoneMuteSupported READ microphoneMuteSupported NOTIFY statusChanged)

public:
    Calls(Phone *phone, Bluetooth *bluetooth);
    ~Calls() override;

    bool gatewayConnected() const;
    bool telephonyAvailable() const;
    // Bluetooth address -> name, for the settings page
    QVariantMap pairedPhones() const;
    QStringList connectedPhones() const;

    /**
     * The settings, with their defaults where not set: useThisComputer, autoConnect,
     * gatewayAddress (the phone's Bluetooth address, empty to find it by name)
     */
    Q_SCRIPTABLE QVariantMap settings() const;
    Q_SCRIPTABLE void setSetting(const QString &key, const QDBusVariant &value);

    /**
     * Calls a phone number or a contact from this computer, with the call audio here.
     * Returns an error message, or an empty string if the call was requested from the phone.
     */
    Q_SCRIPTABLE QString dial(const QString &numberOrName);

    /**
     * Contacts matching query as a JSON array of {name, number}, one entry per phone number
     */
    Q_SCRIPTABLE QString searchContacts(const QString &query) const;

    QString callState() const;
    QString callName() const;
    QString callNumber() const;
    bool audioOnPc() const;
    bool microphoneMuted() const;
    bool microphoneMuteSupported() const;

    Q_SCRIPTABLE void answer();
    Q_SCRIPTABLE void hangUp();
    Q_SCRIPTABLE void moveAudioToPc();
    Q_SCRIPTABLE void sendTones(const QString &tones);
    /**
     * Mutes what the phone hears from this computer, until the call ends
     */
    Q_SCRIPTABLE void setMicrophoneMuted(bool muted);

    /**
     * KDE Connect's telephony signal: event is "callReceived" while ringing or "missedCall"
     */
    void telephonyCallReceived(const QString &event, const QString &number, const QString &contactName);

    struct Contact {
        QString name;
        QStringList numbers;
    };
    /**
     * Up to limit contacts whose full name is query, else whose name contains it.
     * Numbers stored twice, e.g. in national and international form, are listed once.
     */
    QList<Contact> findContacts(const QString &query, int limit) const;
    static bool isPhoneNumber(const QString &text);
    void dialNumber(const QString &number);

Q_SIGNALS:
    Q_SCRIPTABLE void gatewayConnectedChanged(bool connected);
    Q_SCRIPTABLE void statusChanged();
    Q_SCRIPTABLE void callChanged();
    Q_SCRIPTABLE void settingsChanged();

private Q_SLOTS:
    void interfacesAdded(const QDBusMessage &message);
    void interfacesRemoved(const QDBusMessage &message);
    void propertiesChanged(const QDBusMessage &message);

private:
    using InterfaceMap = Bluetooth::InterfaceMap;
    using ObjectMap = Bluetooth::ObjectMap;

    enum class Actions {
        None,
        Ringing,
        RingingLocked,
        Waiting,
        WaitingLocked,
        Call,
        CallOnPc,
        CallMuted,
        CallOnPhone,
        Held,
    };

    struct Call {
        QVariantMap properties;
        QPointer<KNotification> notification;
        QPointer<KNotification> ringtone;
        QString name;
        QPixmap photo;
        Actions actions = Actions::None;
        // Shown on this computer: set when the call appears with "Use this computer" on, or when it is turned on
        bool handledHere = false;
        bool declined = false;
        bool answered = false;
    };

    QVariant setting(const QString &key) const;
    void phoneChanged();
    void bluetoothChanged();
    void useThisComputerChanged();
    void autoConnect();
    bool wantsConnection(const QString &address) const;
    void connectPhone(const QString &address, bool retry);
    QString gatewayAddress() const;
    void resolveGateway();
    void selectGateway(const QString &path);
    void addCall(const QString &path, const InterfaceMap &interfaces);
    void removeCall(const QString &path, const QStringList &interfaces);
    void watchChildren(const QString &path, bool watch);
    void watchProperties(const QString &path, bool watch);
    void resolveCaller(Call &call) const;
    /**
     * The name KDE Connect sent for the call's number, if it came recently
     */
    void applyTelephonyCaller(Call &call) const;
    void updateCallerFromTelephony(const QString &number, const QString &contactName);
    void updateNotification(const QString &path, const QString &previousState);
    void closeNotifications(const Call &call);
    void showMissedCall(const QString &caller, const QString &number);
    void declineCall(const QString &path);
    void refreshNotifications();
    void answerCall(const QString &path);
    QString currentCall() const;
    void sendNextTone();
    void notifyCallChanged();
    void setRejectSco(bool reject);
    void applyMicrophoneMute();
    /**
     * Blocking only when the object is destroyed, as nothing would wait for the answer
     */
    void restoreRejectSco(bool block);
    void callMethod(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments = {});

    Phone *m_phone;
    Bluetooth *m_bluetooth;
    // The phone is gone when the destructor runs
    const QString m_dbusPath;
    KConfigGroup m_config;
    QString m_gatewayPath;
    QString m_gatewayAddress;
    bool m_audioOnPc = false;
    bool m_microphoneMuted = false;
    // Off: only the plain notifications from the phone, the gateway is kept just to keep call audio off this computer
    bool m_useThisComputer = true;
    // DTMF tones still to send; HFP takes one per AT+VTS command
    QString m_pendingTones;
    // What the dialer was last told, to signal only real changes
    QStringList m_lastCallInfo;
    bool m_sendingTone = false;
    // Since the last missed call shown from HFP, to skip KDE Connect's missedCall for the same call
    QElapsedTimer m_hfpMissedCall;
    // The last caller KDE Connect named, as its signal may come before the HFP call
    QString m_telephonyNumber;
    QString m_telephonyName;
    QElapsedTimer m_telephonyTime;
    // Decided once each time the phone comes within KDE Connect's reach
    bool m_autoConnectDecided = false;
    bool m_reachable = false;
    QString m_name;
    // PipeWire call object path -> call
    QHash<QString, Call> m_calls;
    // Call notification from KDE Connect's telephony signal, used without a Bluetooth gateway
    QPointer<KNotification> m_telephonyNotification;
};
