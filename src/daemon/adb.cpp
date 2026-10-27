/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "adb.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include "phonekompaniond_debug.h"

#include <algorithm>

#define ADB_TLS_SERVICE QStringLiteral("_adb-tls-connect._tcp")

Adb::Adb(QObject *parent)
    : QObject(parent)
{
    findTools();
}

Adb::~Adb()
{
    stopTracking();
}

void Adb::findTools()
{
    const QString adb = m_adb;
    const QString scrcpy = m_scrcpy;
    const QStringList missingTools = m_missingTools;

    m_missingTools.clear();
    m_scrcpy = QStandardPaths::findExecutable(QStringLiteral("scrcpy"));
    // Release archives keep adb and scrcpy-server next to scrcpy, packages the server in share/scrcpy
    const QDir scrcpyDir = QFileInfo(QFileInfo(m_scrcpy).canonicalFilePath()).dir();

    // The adb scrcpy uses, as two adb versions stop each other's server
    m_adb = qEnvironmentVariable("ADB");
    if (m_adb.isEmpty() && !m_scrcpy.isEmpty() && QFileInfo(scrcpyDir.filePath(QStringLiteral("adb"))).isExecutable()) {
        m_adb = scrcpyDir.filePath(QStringLiteral("adb"));
    } else if (m_adb.isEmpty()) {
        m_adb = QStandardPaths::findExecutable(QStringLiteral("adb"));
    }
    if (m_adb.isEmpty()) {
        m_missingTools.append(QStringLiteral("adb"));
    }
    if (m_scrcpy.isEmpty()) {
        m_missingTools.append(QStringLiteral("scrcpy"));
    } else {
        QStringList serverPaths{qEnvironmentVariable("SCRCPY_SERVER_PATH")};
        if (serverPaths.constFirst().isEmpty()) {
            serverPaths = {scrcpyDir.filePath(QStringLiteral("../share/scrcpy/scrcpy-server")), scrcpyDir.filePath(QStringLiteral("scrcpy-server"))};
        }
        if (std::none_of(serverPaths.cbegin(), serverPaths.cend(), [](const QString &path) {
                return QFileInfo::exists(path);
            })) {
            m_missingTools.append(QStringLiteral("scrcpy-server"));
        }
    }

    if (m_adb != adb || m_scrcpy != scrcpy || m_missingTools != missingTools) {
        qCDebug(PHONEKOMPANIOND) << "adb" << m_adb << "scrcpy" << m_scrcpy << "missing" << m_missingTools;
        Q_EMIT toolsChanged();
    }
    // Two adb versions stop each other's server
    if (m_adb != adb) {
        stopTracking();
    }
    if (!m_adb.isEmpty() && !m_tracker) {
        trackDevices();
    }
}

QString Adb::adb() const
{
    return m_adb;
}

QString Adb::scrcpy() const
{
    return m_scrcpy;
}

QStringList Adb::missingTools() const
{
    return m_missingTools;
}

QStringList Adb::devices() const
{
    return m_devices;
}

QString Adb::deviceAddress(const QString &serial) const
{
    // "<mDNS service name>._adb-tls-connect._tcp", or "<address>:<port>" when connected by address
    const QString suffix = QLatin1Char('.') + ADB_TLS_SERVICE;
    if (serial.endsWith(suffix)) {
        return m_mdnsAddresses.value(serial.chopped(suffix.size()));
    }
    const int colon = serial.lastIndexOf(QLatin1Char(':'));
    return colon > 0 ? serial.left(colon) : QString();
}

void Adb::trackDevices()
{
    m_tracker = new QProcess(this);
    m_trackerOutput.clear();
    connect(m_tracker, &QProcess::readyReadStandardOutput, this, &Adb::readTrackedDevices);
    connect(m_tracker, &QProcess::finished, this, [this] {
        // E.g. when the adb server was stopped; tracking starts it again
        qCDebug(PHONEKOMPANIOND) << "adb track-devices ended" << m_tracker->readAllStandardError();
        m_tracker->deleteLater();
        m_tracker = nullptr;
        if (!m_devices.isEmpty()) {
            m_devices.clear();
            Q_EMIT devicesChanged();
        }
        QTimer::singleShot(5000, this, [this] {
            if (!m_tracker && !m_adb.isEmpty()) {
                trackDevices();
            }
        });
    });
    // Tried again on the next findTools()
    connect(m_tracker, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(PHONEKOMPANIOND) << "Could not start" << m_adb << m_tracker->errorString();
            m_tracker->deleteLater();
            m_tracker = nullptr;
        }
    });
    m_tracker->start(m_adb, {QStringLiteral("track-devices"), QStringLiteral("-l")});
}

void Adb::stopTracking()
{
    if (!m_tracker) {
        return;
    }
    m_tracker->disconnect(this);
    m_tracker->kill();
    m_tracker->waitForFinished(1000);
    delete m_tracker;
    m_tracker = nullptr;
}

void Adb::readTrackedDevices()
{
    m_trackerOutput += m_tracker->readAllStandardOutput();
    // Each update is the whole device list, prefixed with its length as four hex digits
    bool updated = false;
    while (m_trackerOutput.size() >= 4) {
        bool ok = false;
        const int length = m_trackerOutput.left(4).toInt(&ok, 16);
        if (!ok) {
            qCWarning(PHONEKOMPANIOND) << "Unexpected adb track-devices output" << m_trackerOutput;
            m_trackerOutput.clear();
            return;
        } else if (m_trackerOutput.size() < 4 + length) {
            break;
        }
        const QStringList lines = QString::fromUtf8(m_trackerOutput.mid(4, length)).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        m_trackerOutput.remove(0, 4 + length);

        QStringList devices;
        for (const QString &line : lines) {
            // <serial> <state> <details>; other states are offline, unauthorized etc.
            const QStringList fields = line.simplified().split(QLatin1Char(' '));
            if (fields.size() >= 2 && fields.at(1) == QLatin1String("device")) {
                devices.append(fields.at(0));
            }
        }
        if (devices != m_devices) {
            m_devices = devices;
            updated = true;
        }
    }
    if (updated) {
        qCDebug(PHONEKOMPANIOND) << "adb devices" << m_devices;
        Q_EMIT devicesChanged();
        refreshMdnsServices();
    }
}

void Adb::refreshMdnsServices()
{
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        process->deleteLater();
        QHash<QString, QString> addresses;
        const QStringList lines = QString::fromUtf8(process->readAllStandardOutput()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            // <name> <service type> <address>:<port>
            const QStringList fields = line.split(QLatin1Char('\t'));
            if (fields.size() == 3 && fields.at(1) == ADB_TLS_SERVICE) {
                const QString address = fields.at(2);
                addresses.insert(fields.at(0), address.left(address.lastIndexOf(QLatin1Char(':'))));
            }
        }
        if (addresses != m_mdnsAddresses) {
            m_mdnsAddresses = addresses;
            Q_EMIT devicesChanged();
        }
    });
    connect(process, &QProcess::errorOccurred, process, &QObject::deleteLater);
    process->start(m_adb, {QStringLiteral("mdns"), QStringLiteral("services")});
}

#include "moc_adb.cpp"
