/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "scrcpyinterface.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QJsonArray>
#include <QJsonDocument>

#include "dbuscall.h"
#include "dbusnames.h"

ScrcpyInterface::ScrcpyInterface(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    auto watcher = new QDBusServiceWatcher(PHONEKOMPANION_SERVICE, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        // A new daemon knows nothing of the old one's pairing, and answers to the old one are dropped
        ++m_generation;
        reset();
        if (newOwner.isEmpty()) {
            return;
        }
        readStatus();
        readSettings();
        readApps();
    });
    // Phones come and go with KDE Connect
    bus.connect(PHONEKOMPANION_SERVICE, PHONEKOMPANION_PHONES_PATH, PHONEKOMPANION_PHONES_INTERFACE, QStringLiteral("phonesChanged"), this, SLOT(readStatus()));
}

QString ScrcpyInterface::phoneId() const
{
    return m_phoneId;
}

void ScrcpyInterface::setPhoneId(const QString &phoneId)
{
    if (m_phoneId == phoneId) {
        return;
    }
    connectSignals(false);
    m_phoneId = phoneId;
    ++m_generation;
    reset();
    connectSignals(true);
    Q_EMIT phoneIdChanged();

    readStatus();
    readSettings();
    readApps();
}

bool ScrcpyInterface::isAvailable() const
{
    return !m_status.isEmpty();
}

QStringList ScrcpyInterface::missingTools() const
{
    return m_status.value(QStringLiteral("missingTools")).toStringList();
}

bool ScrcpyInterface::networkReachable() const
{
    return m_status.value(QStringLiteral("networkReachable")).toBool();
}

QStringList ScrcpyInterface::adbDevices() const
{
    return m_status.value(QStringLiteral("adbDevices")).toStringList();
}

QString ScrcpyInterface::adbSerial() const
{
    return m_status.value(QStringLiteral("adbSerial")).toString();
}

QString ScrcpyInterface::webcamDevice() const
{
    return m_status.value(QStringLiteral("webcamDevice")).toString();
}

QString ScrcpyInterface::webcamName() const
{
    return m_status.value(QStringLiteral("webcamName")).toString();
}

bool ScrcpyInterface::webcamActive() const
{
    return m_status.value(QStringLiteral("webcamActive")).toBool();
}

QString ScrcpyInterface::unavailableReason() const
{
    return m_status.value(QStringLiteral("unavailableReason")).toString();
}

QVariantMap ScrcpyInterface::settings() const
{
    return m_settings;
}

QVariantList ScrcpyInterface::apps() const
{
    return m_apps;
}

bool ScrcpyInterface::isRefreshingApps() const
{
    return m_refreshingApps;
}

QString ScrcpyInterface::appsError() const
{
    return m_appsError;
}

bool ScrcpyInterface::isPairing() const
{
    return m_pairing;
}

QString ScrcpyInterface::qrContent() const
{
    return m_qrContent;
}

void ScrcpyInterface::setSetting(const QString &key, const QVariant &value)
{
    call(QStringLiteral("setSetting"), {key, QVariant::fromValue(QDBusVariant(value))});
}

void ScrcpyInterface::mirrorScreen()
{
    callAction(QStringLiteral("mirrorScreen"));
}

void ScrcpyInterface::openApp(const QString &package)
{
    callAction(QStringLiteral("openApp"), {package});
}

void ScrcpyInterface::showCamera(const QString &facing)
{
    callAction(QStringLiteral("showCamera"), {facing});
}

void ScrcpyInterface::startWebcam(const QString &facing)
{
    callAction(QStringLiteral("startWebcam"), {facing});
}

void ScrcpyInterface::stopWebcam()
{
    call(QStringLiteral("stopWebcam"), {});
}

void ScrcpyInterface::refreshApps()
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    m_refreshingApps = true;
    m_appsError.clear();
    Q_EMIT appsChanged();
    call(QStringLiteral("refreshApps"), {}, [this](const QDBusMessage &reply) {
        // An empty answer means appsChanged follows
        const QString reason = reply.type() == QDBusMessage::ReplyMessage ? reply.arguments().value(0).toString() : reply.errorMessage();
        if (!reason.isEmpty()) {
            m_refreshingApps = false;
            m_appsError = reason;
            Q_EMIT appsChanged();
        }
    });
}

void ScrcpyInterface::pair(const QString &address, const QString &code)
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    m_pairing = true;
    Q_EMIT pairingChanged();
    call(QStringLiteral("pair"), {address, code}, [this](const QDBusMessage &reply) {
        if (reply.type() != QDBusMessage::ReplyMessage) {
            onPairingFinished(false, reply.errorMessage());
        }
    });
}

void ScrcpyInterface::startQrPairing()
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    m_pairing = true;
    m_qrStarting = true;
    m_qrStartMessage.clear();
    Q_EMIT pairingChanged();
    call(QStringLiteral("startQrPairing"), {}, [this](const QDBusMessage &reply) {
        m_qrStarting = false;
        const QString content = reply.type() == QDBusMessage::ReplyMessage ? reply.arguments().value(0).toString() : QString();
        if (reply.type() != QDBusMessage::ReplyMessage) {
            onPairingFinished(false, reply.errorMessage());
        } else if (content.isEmpty()) {
            // Pairing could not start, and pairingFinished came first to say why
            onPairingFinished(false, m_qrStartMessage);
        } else if (m_pairing) {
            m_qrContent = content;
            Q_EMIT pairingChanged();
        }
        m_qrStartMessage.clear();
    });
}

void ScrcpyInterface::stopQrPairing()
{
    call(QStringLiteral("stopQrPairing"), {});
}

void ScrcpyInterface::readStatus()
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    const quint64 statusGeneration = ++m_statusGeneration;
    auto watcher = new QDBusPendingCallWatcher(getAllProperties(path(), PHONEKOMPANION_SCRCPY_INTERFACE), this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, statusGeneration](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (generation != m_generation || statusGeneration != m_statusGeneration) {
            return;
        }
        const QDBusMessage reply = watcher->reply();
        const QVariantMap status = reply.type() == QDBusMessage::ReplyMessage ? qdbus_cast<QVariantMap>(reply.arguments().value(0)) : QVariantMap();
        if (status != m_status) {
            m_status = status;
            Q_EMIT statusChanged();
        }
        if (status.isEmpty() && m_pairing) {
            onPairingFinished(false, QString());
        }
    });
}

void ScrcpyInterface::readSettings()
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    call(QStringLiteral("settings"), {}, [this](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ReplyMessage) {
            m_settings = qdbus_cast<QVariantMap>(reply.arguments().value(0));
            Q_EMIT settingsChanged();
        }
    });
}

void ScrcpyInterface::readApps()
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    call(QStringLiteral("apps"), {}, [this](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ReplyMessage) {
            m_apps = QJsonDocument::fromJson(reply.arguments().value(0).toString().toUtf8()).array().toVariantList();
        }
        Q_EMIT appsChanged();
    });
}

void ScrcpyInterface::onAppsChanged()
{
    m_refreshingApps = false;
    readApps();
}

void ScrcpyInterface::onPairingFinished(bool success, const QString &message)
{
    // Also sent for pairing started elsewhere, e.g. in another window
    if (!m_pairing) {
        return;
    }
    // Before startQrPairing() answers, this is about the code it replaced, or why it failed
    if (m_qrStarting) {
        m_qrStartMessage = message;
        return;
    }
    m_pairing = false;
    m_qrContent.clear();
    Q_EMIT pairingChanged();
    if (!message.isEmpty()) {
        Q_EMIT pairingFinished(success, message);
    }
}

QString ScrcpyInterface::path() const
{
    return PHONEKOMPANION_PHONES_PATH + QLatin1Char('/') + m_phoneId + QStringLiteral("/scrcpy");
}

void ScrcpyInterface::connectSignals(bool connect)
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    QDBusConnection bus = QDBusConnection::sessionBus();
    const std::pair<const char *, const char *> signalSlots[] = {
        {"statusChanged", SLOT(readStatus())},
        {"settingsChanged", SLOT(readSettings())},
        {"appsChanged", SLOT(onAppsChanged())},
        {"pairingFinished", SLOT(onPairingFinished(bool, QString))},
    };
    for (const auto &[signal, slot] : signalSlots) {
        if (connect) {
            bus.connect(PHONEKOMPANION_SERVICE, path(), PHONEKOMPANION_SCRCPY_INTERFACE, QLatin1String(signal), this, slot);
        } else {
            bus.disconnect(PHONEKOMPANION_SERVICE, path(), PHONEKOMPANION_SCRCPY_INTERFACE, QLatin1String(signal), this, slot);
        }
    }
}

void ScrcpyInterface::call(const QString &method, const QVariantList &arguments, const std::function<void(const QDBusMessage &)> &callback)
{
    if (m_phoneId.isEmpty()) {
        return;
    }
    auto watcher = new QDBusPendingCallWatcher(callDaemon(path(), PHONEKOMPANION_SCRCPY_INTERFACE, method, arguments), this);
    const quint64 generation = m_generation;
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation, callback](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (generation == m_generation && callback) {
            callback(watcher->reply());
        }
    });
}

void ScrcpyInterface::callAction(const QString &method, const QVariantList &arguments)
{
    call(method, arguments, [this](const QDBusMessage &reply) {
        const QString message = reply.type() == QDBusMessage::ReplyMessage ? reply.arguments().value(0).toString() : reply.errorMessage();
        if (!message.isEmpty()) {
            Q_EMIT error(message);
        }
    });
}

void ScrcpyInterface::reset()
{
    m_status.clear();
    m_settings.clear();
    m_apps.clear();
    m_refreshingApps = false;
    m_appsError.clear();
    m_pairing = false;
    m_qrStarting = false;
    m_qrStartMessage.clear();
    m_qrContent.clear();
    Q_EMIT statusChanged();
    Q_EMIT settingsChanged();
    Q_EMIT appsChanged();
    Q_EMIT pairingChanged();
}

#include "moc_scrcpyinterface.cpp"
