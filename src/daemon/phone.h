/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantMap>

class Calls;
class Scrcpy;

/**
 * A phone or tablet paired in KDE Connect, at /phones/<KDE Connect device id>
 */
class Phone : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.rkrajnc.phonekompanion.phone")
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(bool reachable READ isReachable)
    // Connected over Bluetooth for calls, and calls are on for this computer
    Q_PROPERTY(bool callsConnected READ callsConnected)
    // KDE Connect's SMS plugin is loaded for the phone, which needs it reachable
    Q_PROPERTY(bool messagesAvailable READ messagesAvailable)

public:
    Phone(const QString &id, QObject *parent = nullptr);
    ~Phone() override;

    QString id() const;
    QString name() const;
    bool isReachable() const;
    bool callsConnected() const;
    bool messagesAvailable() const;
    /**
     * The phone's IP addresses, while KDE Connect reaches it over the network
     */
    QStringList addresses() const;
    QString dbusPath() const;
    /**
     * Shows an error as a notification, titled with the phone's name or title; nothing for an
     * empty error
     */
    void notifyError(const QString &error, const QString &title = QString()) const;
    /**
     * Mirroring, set once by PhoneManager
     */
    Scrcpy *scrcpy() const;
    void setScrcpy(Scrcpy *scrcpy);
    /**
     * Bluetooth calls, set once by PhoneManager
     */
    Calls *calls() const;
    void setCalls(Calls *calls);

    /**
     * Takes the KDE Connect device properties, returns whether a property on D-Bus changed
     */
    bool update(const QVariantMap &properties);

Q_SIGNALS:
    void changed();

private:
    const QString m_id;
    QString m_name;
    bool m_reachable = false;
    bool m_messagesAvailable = false;
    QStringList m_addresses;
    Scrcpy *m_scrcpy = nullptr;
    Calls *m_calls = nullptr;
};
