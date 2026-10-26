/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QTimer>

class KdeConnect;
class Phone;

/**
 * Keeps a Phone for every phone or tablet paired in KDE Connect, at /phones
 */
class PhoneManager : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.rkrajnc.phonekompanion.phones")
    Q_PROPERTY(QStringList phones READ phones)
    Q_PROPERTY(bool kdeConnectRunning READ isKdeConnectRunning)

public:
    explicit PhoneManager(QObject *parent = nullptr);
    ~PhoneManager() override;

    QList<Phone *> allPhones() const;
    /**
     * The KDE Connect device ids of the phones
     */
    QStringList phones() const;
    bool isKdeConnectRunning() const;

Q_SIGNALS:
    /**
     * A phone was added or removed, or a property here or of a phone changed
     */
    Q_SCRIPTABLE void phonesChanged();

private:
    void refresh();

    KdeConnect *m_kdeConnect;
    QHash<QString, Phone *> m_phones;
    bool m_kdeConnectRunning = false;
    QTimer m_refreshTimer;
};
