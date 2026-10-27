/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phonemanager.h"

#include <QDBusConnection>
#include <QSet>

#include "adb.h"
#include "bluetooth.h"
#include "calls.h"
#include "dbusnames.h"
#include "kdeconnect.h"
#include "phone.h"
#include "phonekompaniond_debug.h"
#include "scrcpy.h"

PhoneManager::PhoneManager(QObject *parent)
    : QObject(parent)
    , m_kdeConnect(new KdeConnect(this))
    , m_adb(new Adb(this))
    , m_bluetooth(new Bluetooth(this))
{
    // KDE Connect sends several signals when a device connects; read the devices once
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(100);
    connect(&m_refreshTimer, &QTimer::timeout, this, &PhoneManager::refresh);
    connect(m_kdeConnect, &KdeConnect::devicesChanged, &m_refreshTimer, qOverload<>(&QTimer::start));
    connect(m_kdeConnect, &KdeConnect::callReceived, this, [this](const QString &id, const QString &event, const QString &number, const QString &contactName) {
        if (Phone *phone = m_phones.value(id)) {
            phone->calls()->telephonyCallReceived(event, number, contactName);
        }
    });

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
    if (!m_kdeConnectRunning) {
        // Only KDE Connect lost track of them; keep the phones and their mirroring windows
        for (Phone *phone : std::as_const(m_phones)) {
            changed |= phone->update({{QStringLiteral("name"), phone->name()}});
        }
        if (changed) {
            Q_EMIT phonesChanged();
        }
        return;
    }

    QSet<QString> found;
    bool connected = false;

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
            phone->setScrcpy(new Scrcpy(phone, m_adb));
            phone->setCalls(new Calls(phone, m_bluetooth));
            changed = true;
        }
        const bool wasReachable = phone->isReachable();
        changed |= phone->update(properties);
        connected |= phone->isReachable() && !wasReachable;
    }

    for (auto it = m_phones.begin(); it != m_phones.end();) {
        if (found.contains(it.key())) {
            ++it;
            continue;
        }
        qCDebug(PHONEKOMPANIOND) << "Phone removed" << it.key();
        // Unpaired, so its launchers would only fail
        it.value()->scrcpy()->removeLaunchers();
        delete it.value();
        it = m_phones.erase(it);
        changed = true;
    }

    m_bluetooth->setPhoneCount(m_phones.size());

    // The tools may have been installed while no phone was connected
    if (connected) {
        m_adb->findTools();
    }
    if (changed) {
        Q_EMIT phonesChanged();
    }
}

#include "moc_phonemanager.cpp"
