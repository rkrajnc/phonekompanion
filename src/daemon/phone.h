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

public:
    Phone(const QString &id, QObject *parent = nullptr);
    ~Phone() override;

    QString id() const;
    QString name() const;
    bool isReachable() const;
    /**
     * The phone's IP addresses, while KDE Connect reaches it over the network
     */
    QStringList addresses() const;
    QString dbusPath() const;
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
    QStringList m_addresses;
    Scrcpy *m_scrcpy = nullptr;
    Calls *m_calls = nullptr;
};
