/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phonemanager.h"

#include <QDBusConnection>
#include <QSet>

#include "dbusnames.h"
#include "kdeconnect.h"
#include "phone.h"
#include "phonekompaniond_debug.h"

PhoneManager::PhoneManager(QObject *parent)
    : QObject(parent)
    , m_kdeConnect(new KdeConnect(this))
{
    // KDE Connect sends several signals when a device connects; read the devices once
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(100);
    connect(&m_refreshTimer, &QTimer::timeout, this, &PhoneManager::refresh);
    connect(m_kdeConnect, &KdeConnect::devicesChanged, &m_refreshTimer, qOverload<>(&QTimer::start));

    QDBusConnection::sessionBus().registerObject(PHONEKOMPANION_PHONES_PATH, this, QDBusConnection::ExportScriptableContents);
    refresh();
}

PhoneManager::~PhoneManager()
{
    QDBusConnection::sessionBus().unregisterObject(PHONEKOMPANION_PHONES_PATH);
}

QList<Phone *> PhoneManager::allPhones() const
{
    return m_phones.values();
}

QStringList PhoneManager::phones() const
{
    return m_phones.keys();
}

bool PhoneManager::isKdeConnectRunning() const
{
    return m_kdeConnectRunning;
}

void PhoneManager::refresh()
{
    const std::optional<QStringList> ids = m_kdeConnect->pairedDevices();
    if (!ids) {
        // Keep the phones until KDE Connect answers again
        return;
    }

    bool changed = m_kdeConnectRunning != m_kdeConnect->isRunning();
    m_kdeConnectRunning = m_kdeConnect->isRunning();
    QSet<QString> found;

    for (const QString &id : *ids) {
        const QVariantMap properties = m_kdeConnect->deviceProperties(id);
        if (properties.isEmpty()) {
            if (m_phones.contains(id)) {
                found.insert(id);
            }
            continue;
        }
        const QString type = properties.value(QStringLiteral("type")).toString();
        if (type != QLatin1String("phone") && type != QLatin1String("tablet")) {
            continue;
        }

        found.insert(id);
        Phone *&phone = m_phones[id];
        if (!phone) {
            qCDebug(PHONEKOMPANIOND) << "Phone added" << id;
            phone = new Phone(id, this);
            changed = true;
        }
        changed |= phone->update(properties);
    }

    for (auto it = m_phones.begin(); it != m_phones.end();) {
        if (found.contains(it.key())) {
            ++it;
            continue;
        }
        qCDebug(PHONEKOMPANIOND) << "Phone removed" << it.key();
        delete it.value();
        it = m_phones.erase(it);
        changed = true;
    }

    if (changed) {
        Q_EMIT phonesChanged();
    }
}

#include "moc_phonemanager.cpp"
