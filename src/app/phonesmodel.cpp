/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "phonesmodel.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QProcess>

#include <algorithm>
#include <memory>

#include "dbuscall.h"
#include "dbusnames.h"
#include "findexecutable.h"

PhonesModel::PhonesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();

    auto watcher = new QDBusServiceWatcher(PHONEKOMPANION_SERVICE, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        m_serviceRunning = !newOwner.isEmpty();
        Q_EMIT serviceRunningChanged();
        refresh();
    });
    bus.connect(PHONEKOMPANION_SERVICE, PHONEKOMPANION_PHONES_PATH, PHONEKOMPANION_PHONES_INTERFACE, QStringLiteral("phonesChanged"), this, SLOT(refresh()));

    m_serviceRunning = bus.interface()->isServiceRegistered(PHONEKOMPANION_SERVICE);
    if (m_serviceRunning) {
        refresh();
    } else {
        startService();
    }
}

int PhonesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_phones.size();
}

QVariant PhonesModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }

    const Phone &phone = m_phones.at(index.row());
    switch (role) {
    case PhoneIdRole:
        return phone.id;
    case Qt::DisplayRole:
    case NameRole:
        return phone.name;
    case ReachableRole:
        return phone.reachable;
    }
    return {};
}

QHash<int, QByteArray> PhonesModel::roleNames() const
{
    return {
        {PhoneIdRole, "phoneId"},
        {NameRole, "name"},
        {ReachableRole, "reachable"},
    };
}

bool PhonesModel::isServiceRunning() const
{
    return m_serviceRunning;
}

bool PhonesModel::isKdeConnectRunning() const
{
    return m_kdeConnectRunning;
}

bool PhonesModel::isLoaded() const
{
    return m_loaded;
}

int PhonesModel::indexOf(const QString &phoneId) const
{
    const auto it = std::find_if(m_phones.cbegin(), m_phones.cend(), [&phoneId](const Phone &phone) {
        return phone.id == phoneId;
    });
    return it == m_phones.cend() ? -1 : it - m_phones.cbegin();
}

void PhonesModel::startService()
{
    const QString daemon = findOwnExecutable(QStringLiteral("phonekompaniond"));
    if (daemon.isEmpty() || !QProcess::startDetached(daemon, {})) {
        qWarning() << "Could not start phonekompaniond" << daemon;
    }
}

void PhonesModel::openKdeConnect()
{
    if (!QProcess::startDetached(QStringLiteral("kdeconnect-app"), {})) {
        qWarning() << "Could not start kdeconnect-app";
    }
}

void PhonesModel::refresh()
{
    const quint64 generation = ++m_generation;
    if (!m_serviceRunning) {
        setPhones({}, false);
        return;
    }

    auto watcher = new QDBusPendingCallWatcher(getAllProperties(PHONEKOMPANION_PHONES_PATH, PHONEKOMPANION_PHONES_INTERFACE), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        if (generation != m_generation || !reply.isValid()) {
            return;
        }

        struct Pending {
            QList<Phone> phones;
            qsizetype remaining;
            bool kdeConnectRunning;
        };
        const QStringList ids = reply.value().value(QStringLiteral("phones")).toStringList();
        auto pending = std::make_shared<Pending>(QList<Phone>(), ids.size(), reply.value().value(QStringLiteral("kdeConnectRunning")).toBool());
        if (ids.isEmpty()) {
            setPhones({}, pending->kdeConnectRunning);
            return;
        }

        for (const QString &id : ids) {
            auto phoneWatcher =
                new QDBusPendingCallWatcher(getAllProperties(PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + id, PHONEKOMPANION_PHONE_INTERFACE), this);
            connect(phoneWatcher, &QDBusPendingCallWatcher::finished, this, [this, generation, id, pending](QDBusPendingCallWatcher *phoneWatcher) {
                phoneWatcher->deleteLater();
                const QDBusPendingReply<QVariantMap> phoneReply = *phoneWatcher;
                if (phoneReply.isValid()) {
                    const QVariantMap properties = phoneReply.value();
                    pending->phones.append({id, properties.value(QStringLiteral("name")).toString(), properties.value(QStringLiteral("reachable")).toBool()});
                }
                if (--pending->remaining == 0 && generation == m_generation) {
                    setPhones(pending->phones, pending->kdeConnectRunning);
                }
            });
        }
    });
}

void PhonesModel::setPhones(QList<Phone> phones, bool kdeConnectRunning)
{
    std::sort(phones.begin(), phones.end(), [](const Phone &a, const Phone &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });

    // Before the reset, for its handlers
    const bool wasLoaded = m_loaded;
    m_loaded = m_serviceRunning;

    beginResetModel();
    m_phones = phones;
    endResetModel();

    if (m_loaded != wasLoaded) {
        Q_EMIT loadedChanged();
    }

    if (m_kdeConnectRunning != kdeConnectRunning) {
        m_kdeConnectRunning = kdeConnectRunning;
        Q_EMIT kdeConnectRunningChanged();
    }
}

#include "moc_phonesmodel.cpp"
