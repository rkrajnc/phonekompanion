/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "scrcpyinterface.h"

#include <QDBusMessage>
#include <QJsonArray>
#include <QJsonDocument>

#include "dbusnames.h"

ScrcpyInterface::ScrcpyInterface(QObject *parent)
    : PhoneFeature(QStringLiteral("scrcpy"), PHONEKOMPANION_SCRCPY_INTERFACE, parent)
{
}

QStringList ScrcpyInterface::missingTools() const
{
    return status(QStringLiteral("missingTools")).toStringList();
}

bool ScrcpyInterface::networkReachable() const
{
    return status(QStringLiteral("networkReachable")).toBool();
}

QStringList ScrcpyInterface::adbDevices() const
{
    return status(QStringLiteral("adbDevices")).toStringList();
}

QString ScrcpyInterface::adbSerial() const
{
    return status(QStringLiteral("adbSerial")).toString();
}

QString ScrcpyInterface::webcamDevice() const
{
    return status(QStringLiteral("webcamDevice")).toString();
}

QString ScrcpyInterface::webcamName() const
{
    return status(QStringLiteral("webcamName")).toString();
}

bool ScrcpyInterface::webcamActive() const
{
    return status(QStringLiteral("webcamActive")).toBool();
}

QString ScrcpyInterface::unavailableReason() const
{
    return status(QStringLiteral("unavailableReason")).toString();
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

QStringList ScrcpyInterface::launchers() const
{
    return m_launchers;
}

bool ScrcpyInterface::isPairing() const
{
    return m_pairing;
}

QString ScrcpyInterface::qrContent() const
{
    return m_qrContent;
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
    if (!hasPhone()) {
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

void ScrcpyInterface::setLauncher(const QString &package, bool enabled)
{
    callAction(QStringLiteral("setLauncher"), {package, enabled});
}

void ScrcpyInterface::pair(const QString &address, const QString &code)
{
    if (!hasPhone()) {
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
    if (!hasPhone()) {
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

void ScrcpyInterface::readApps()
{
    if (!hasPhone()) {
        return;
    }
    call(QStringLiteral("apps"), {}, [this](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ReplyMessage) {
            m_apps = QJsonDocument::fromJson(reply.arguments().value(0).toString().toUtf8()).array().toVariantList();
        }
        Q_EMIT appsChanged();
    });
}

void ScrcpyInterface::readLaunchers()
{
    if (!hasPhone()) {
        return;
    }
    call(QStringLiteral("launchers"), {}, [this](const QDBusMessage &reply) {
        if (reply.type() == QDBusMessage::ReplyMessage) {
            m_launchers = reply.arguments().value(0).toStringList();
            Q_EMIT launchersChanged();
        }
    });
}

void ScrcpyInterface::onAppsChanged(const QString &error)
{
    m_refreshingApps = false;
    m_appsError = error;
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

void ScrcpyInterface::readMore()
{
    readApps();
    readLaunchers();
}

void ScrcpyInterface::reset()
{
    m_apps.clear();
    m_launchers.clear();
    m_refreshingApps = false;
    m_appsError.clear();
    m_pairing = false;
    m_qrStarting = false;
    m_qrStartMessage.clear();
    m_qrContent.clear();
    PhoneFeature::reset();
    Q_EMIT appsChanged();
    Q_EMIT launchersChanged();
    Q_EMIT pairingChanged();
}

QList<PhoneFeature::SignalSlot> ScrcpyInterface::signalSlots() const
{
    return {
        {"appsChanged", SLOT(onAppsChanged(QString))},
        {"launchersChanged", SLOT(readLaunchers())},
        {"pairingFinished", SLOT(onPairingFinished(bool, QString))},
    };
}

void ScrcpyInterface::statusRead()
{
    // The object is gone, and with it the pairing
    if (!isAvailable() && m_pairing) {
        onPairingFinished(false, QString());
    }
}

#include "moc_scrcpyinterface.cpp"
