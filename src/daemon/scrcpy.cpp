/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "scrcpy.h"

#include <KLocalizedString>
#include <KSharedConfig>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTimer>

#include "adb.h"
#include "phone.h"
#include "phonekompaniond_debug.h"

#include <algorithm>

#define ADB_PAIRING_SERVICE QStringLiteral("_adb-tls-pairing._tcp")
#define KWIN_SERVICE QStringLiteral("org.kde.KWin")
#define KWIN_SCRIPTING_INTERFACE QStringLiteral("org.kde.kwin.Scripting")
#define KWIN_FOCUS_SCRIPT QStringLiteral("phonekompanion_focus_")

namespace
{
// The last "ERROR: " line of scrcpy's output, e.g. when the phone went away
QString scrcpyError(const QStringList &lines)
{
    QString error;
    for (const QString &line : lines) {
        if (line.startsWith(QLatin1String("ERROR: "))) {
            error = line.mid(7);
        }
    }
    return error;
}

const QVariantMap &defaultSettings()
{
    static const QVariantMap defaults{
        {QStringLiteral("adbSerial"), QString()},
        {QStringLiteral("keyboard"), QString()},
        {QStringLiteral("turnScreenOff"), true},
        {QStringLiteral("audio"), true},
        {QStringLiteral("extraOptions"), QString()},
        {QStringLiteral("appDisplay"), QStringLiteral("phone")},
        {QStringLiteral("appDensity"), 240},
        {QStringLiteral("camera"), QStringLiteral("back")},
        {QStringLiteral("cameraRotation"), 0},
    };
    return defaults;
}
}

Scrcpy::Scrcpy(Phone *phone, Adb *adb)
    : QObject(phone)
    , m_phone(phone)
    , m_adb(adb)
    , m_dbusPath(phone->dbusPath() + QStringLiteral("/scrcpy"))
    , m_config(KSharedConfig::openConfig(QStringLiteral("phonekompanionrc"))->group(phone->id()).group(QStringLiteral("Scrcpy")))
    , m_state(KSharedConfig::openStateConfig(QStringLiteral("phonekompanionstaterc"))->group(phone->id()).group(QStringLiteral("Scrcpy")))
{
    findWebcam();
    connect(m_phone, &Phone::changed, this, [this] {
        // E.g. v4l2loopback was loaded while the phone was away
        if (m_phone->isReachable() && findWebcam()) {
            Q_EMIT statusChanged();
        }
        matchDevice();
    });
    connect(m_adb, &Adb::toolsChanged, this, &Scrcpy::statusChanged);
    connect(m_adb, &Adb::devicesChanged, this, [this] {
        matchDevice(true);
    });
    matchDevice();

    QDBusConnection::sessionBus().registerObject(m_dbusPath, this, QDBusConnection::ExportScriptableContents);
}

Scrcpy::~Scrcpy()
{
    QDBusConnection::sessionBus().unregisterObject(m_dbusPath);
    if (m_appsRequest) {
        m_appsRequest->disconnect(this);
        m_appsRequest->kill();
        m_appsRequest->waitForFinished(1000);
    }
    // All at once, as each window takes a moment to close
    if (m_webcamNotification) {
        m_webcamNotification->close();
    }
    QList<QProcess *> processes = m_windows.values();
    if (m_webcam) {
        processes.append(m_webcam);
    }
    for (QProcess *process : std::as_const(processes)) {
        process->disconnect(this);
        process->terminate();
    }
    for (QProcess *process : std::as_const(processes)) {
        if (!process->waitForFinished(1000)) {
            process->kill();
            process->waitForFinished(1000);
        }
    }
}

QVariant Scrcpy::setting(const QString &key) const
{
    return m_config.readEntry(key, defaultSettings().value(key));
}

QVariantMap Scrcpy::settings() const
{
    QVariantMap settings;
    for (auto it = defaultSettings().cbegin(); it != defaultSettings().cend(); ++it) {
        settings.insert(it.key(), setting(it.key()));
    }
    return settings;
}

void Scrcpy::setSetting(const QString &key, const QDBusVariant &value)
{
    QVariant converted = value.variant();
    if (!defaultSettings().contains(key) || !converted.convert(defaultSettings().value(key).metaType())) {
        qCWarning(PHONEKOMPANIOND) << "Ignoring the setting" << key << value.variant();
        return;
    }
    m_config.writeEntry(key, converted);
    m_config.sync();
    Q_EMIT settingsChanged();
    if (key == QLatin1String("adbSerial")) {
        matchDevice();
    }
}

bool Scrcpy::findWebcam()
{
    // Not while scrcpy writes to it
    if (m_webcam) {
        return false;
    }
    const QString webcamDevice = m_webcamDevice;
    m_webcamDevice.clear();
    m_webcamName.clear();
    // Only v4l2loopback devices have max_openers; the lowest number first
    const QDir devices(QStringLiteral("/sys/devices/virtual/video4linux"));
    QStringList entries = devices.entryList({QStringLiteral("video*")}, QDir::Dirs);
    std::sort(entries.begin(), entries.end(), [](const QString &a, const QString &b) {
        return a.mid(5).toInt() < b.mid(5).toInt();
    });
    for (const QString &entry : std::as_const(entries)) {
        if (!QFileInfo::exists(devices.filePath(entry + QStringLiteral("/max_openers")))) {
            continue;
        }
        m_webcamDevice = QStringLiteral("/dev/") + entry;
        QFile name(devices.filePath(entry + QStringLiteral("/name")));
        if (name.open(QIODevice::ReadOnly)) {
            m_webcamName = QString::fromUtf8(name.readAll()).trimmed();
        }
        break;
    }
    return m_webcamDevice != webcamDevice;
}

void Scrcpy::matchDevice(bool devicesChanged)
{
    // The Bluetooth link's address is not an IP address
    QList<QHostAddress> addresses;
    const QStringList phoneAddresses = m_phone->addresses();
    for (const QString &address : phoneAddresses) {
        const QHostAddress host(address);
        if (!host.isNull()) {
            addresses.append(host);
        }
    }

    QString serial = setting(QStringLiteral("adbSerial")).toString();
    const QStringList adbDevices = m_adb->devices();
    if (!serial.isEmpty()) {
        if (!adbDevices.contains(serial)) {
            serial.clear();
        }
    } else {
        for (const QString &candidate : adbDevices) {
            const QHostAddress host(m_adb->deviceAddress(candidate));
            if (std::any_of(addresses.cbegin(), addresses.cend(), [&host](const QHostAddress &address) {
                    return address.isEqual(host, QHostAddress::ConvertV4MappedToIPv4);
                })) {
                serial = candidate;
                break;
            }
        }
    }

    const bool networkReachable = !addresses.isEmpty();
    if (serial != m_adbSerial || networkReachable != m_networkReachable) {
        qCDebug(PHONEKOMPANIOND) << "adb device for" << m_phone->name() << serial << "network reachable" << networkReachable;
        m_adbSerial = serial;
        m_networkReachable = networkReachable;
        Q_EMIT statusChanged();
    } else if (devicesChanged) {
        Q_EMIT statusChanged();
    }
}

void Scrcpy::pair(const QString &address, const QString &code)
{
    static const QRegularExpression addressPattern(QStringLiteral("^\\S+:\\d+$"));
    static const QRegularExpression codePattern(QStringLiteral("^\\d{6}$"));
    if (m_adb->adb().isEmpty()) {
        Q_EMIT pairingFinished(false, i18n("adb is not installed."));
        return;
    } else if (!addressPattern.match(address.trimmed()).hasMatch()) {
        Q_EMIT pairingFinished(false, i18n("Enter the IP address and port shown on the phone, for example 192.168.1.10:37123."));
        return;
    } else if (!codePattern.match(code.trimmed()).hasMatch()) {
        Q_EMIT pairingFinished(false, i18n("Enter the six-digit pairing code shown on the phone."));
        return;
    }
    // The phone closes its pairing dialog after a failed attempt
    runPair(address.trimmed(),
            code.trimmed(),
            i18n("Pairing failed. Open \"Pair device with pairing code\" on the phone again and use the new address and code."));
}

void Scrcpy::runPair(const QString &address, const QString &code, const QString &retryMessage)
{
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    connect(process, &QProcess::finished, this, [this, process, retryMessage](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        const QString output = QString::fromUtf8(process->readAll()).trimmed();
        qCDebug(PHONEKOMPANIOND) << "adb pair:" << output;
        if (exitStatus == QProcess::NormalExit && exitCode == 0 && output.contains(QLatin1String("Successfully paired"))) {
            Q_EMIT pairingFinished(true, i18n("Paired. The phone connects whenever its Wireless debugging is on."));
            return;
        }
        QString message = retryMessage;
        if (!output.isEmpty()) {
            message += QLatin1Char('\n') + output;
        }
        Q_EMIT pairingFinished(false, message);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            Q_EMIT pairingFinished(false, i18n("Could not start %1.", m_adb->adb()));
        }
    });
    process->start(m_adb->adb(), {QStringLiteral("pair"), address, code});
    // A wrong address can leave adb waiting for minutes
    QTimer::singleShot(30000, process, &QProcess::kill);
}

QString Scrcpy::startQrPairing()
{
    if (m_adb->adb().isEmpty()) {
        Q_EMIT pairingFinished(false, i18n("adb is not installed."));
        return QString();
    }
    // Letters and digits only, as the QR code's format reserves some characters
    const auto randomText = [](int length) {
        static const QLatin1String characters("abcdefghijklmnopqrstuvwxyz0123456789");
        QString text;
        for (int i = 0; i < length; ++i) {
            text.append(characters.at(QRandomGenerator::system()->bounded(int(characters.size()))));
        }
        return text;
    };
    stopQrPairing();
    m_qrName = QStringLiteral("phonekompanion-") + randomText(10);
    m_qrPassword = randomText(12);
    m_qrTime.start();
    pollQrPairing();
    return QStringLiteral("WIFI:T:ADB;S:%1;P:%2;;").arg(m_qrName, m_qrPassword);
}

void Scrcpy::stopQrPairing()
{
    // Other windows may still show the code
    if (!m_qrName.isEmpty()) {
        endQrPairing();
        Q_EMIT pairingFinished(false, i18n("Pairing with the QR code was cancelled."));
    }
}

void Scrcpy::endQrPairing()
{
    m_qrName.clear();
    m_qrPassword.clear();
}

void Scrcpy::pollQrPairing()
{
    // After scanning, the phone offers pairing as an mDNS service named as in the code
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process, name = m_qrName](int exitCode, QProcess::ExitStatus exitStatus) {
        process->deleteLater();
        // Stopped, or replaced by a new code
        if (name != m_qrName) {
            return;
        }
        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            endQrPairing();
            QString message = i18n("adb could not look for the phone.");
            const QString output = QString::fromUtf8(process->readAllStandardError()).trimmed();
            if (!output.isEmpty()) {
                message += QLatin1Char('\n') + output;
            }
            Q_EMIT pairingFinished(false, message);
            return;
        }
        const QStringList lines = QString::fromUtf8(process->readAllStandardOutput()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const QStringList fields = line.trimmed().split(QLatin1Char('\t'));
            if (fields.size() == 3 && fields.at(0) == name && fields.at(1) == ADB_PAIRING_SERVICE) {
                const QString password = m_qrPassword;
                endQrPairing();
                runPair(fields.at(2), password, i18n("Pairing failed. Show a new QR code and scan it again."));
                return;
            }
        }
        if (m_qrTime.hasExpired(120000)) {
            endQrPairing();
            Q_EMIT pairingFinished(false, i18n("No phone scanned the QR code. Show a new code to try again."));
            return;
        }
        QTimer::singleShot(1000, this, [this, name] {
            if (name == m_qrName) {
                pollQrPairing();
            }
        });
    });
    connect(process, &QProcess::errorOccurred, this, [this, process, name = m_qrName](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            if (name == m_qrName) {
                endQrPairing();
                Q_EMIT pairingFinished(false, i18n("Could not start %1.", m_adb->adb()));
            }
        }
    });
    process->start(m_adb->adb(), {QStringLiteral("mdns"), QStringLiteral("services")});
    QTimer::singleShot(10000, process, &QProcess::kill);
}

QString Scrcpy::unavailableReason() const
{
    if (!m_adb->missingTools().isEmpty()) {
        return i18n("Not installed: %1", m_adb->missingTools().join(QStringLiteral(", ")));
    } else if (!m_networkReachable) {
        return i18n("%1 is not on the same network as this computer.", m_phone->name());
    } else if (m_adbSerial.isEmpty()) {
        return i18n("Turn on Wireless debugging on %1, in Developer options or with the Wireless debugging quick settings tile.", m_phone->name());
    }
    return QString();
}

QStringList Scrcpy::scrcpyArguments() const
{
    QStringList arguments{QStringLiteral("--serial=") + m_adbSerial};
    if (setting(QStringLiteral("keyboard")).toString() == QLatin1String("uhid")) {
        arguments.append(QStringLiteral("--keyboard=uhid"));
    }
    return arguments;
}

QString Scrcpy::mirrorScreen()
{
    const QString reason = unavailableReason();
    if (!reason.isEmpty()) {
        return reason;
    }
    QStringList arguments = scrcpyArguments();
    if (setting(QStringLiteral("turnScreenOff")).toBool()) {
        arguments.append(QStringLiteral("--turn-screen-off"));
    }
    startWindow(QStringLiteral("screen"), m_phone->name(), arguments);
    return QString();
}

QString Scrcpy::openApp(const QString &package)
{
    static const QRegularExpression packagePattern(QRegularExpression::anchoredPattern(QStringLiteral("[A-Za-z0-9_]+(\\.[A-Za-z0-9_]+)+")));
    const QString reason = unavailableReason();
    if (!reason.isEmpty()) {
        return reason;
    } else if (!packagePattern.match(package).hasMatch()) {
        return i18n("%1 is not an app package name.", package);
    }

    QString name = package;
    const QJsonArray apps = QJsonDocument::fromJson(this->apps().toUtf8()).array();
    for (const QJsonValue &app : apps) {
        if (app[QStringLiteral("package")].toString() == package) {
            name = app[QStringLiteral("name")].toString();
            break;
        }
    }

    QStringList arguments = scrcpyArguments();
    const QString display = setting(QStringLiteral("appDisplay")).toString();
    if (display == QLatin1String("portrait") || display == QLatin1String("landscape")) {
        // Phone-shaped, so that apps use their phone layout; the window resizes it afterwards
        const int density = setting(QStringLiteral("appDensity")).toInt();
        const QString size = display == QLatin1String("landscape") ? QStringLiteral("1584x720") : QStringLiteral("720x1584");
        arguments << QStringLiteral("--new-display=%1/%2").arg(size, QString::number(density)) << QStringLiteral("--flex-display");
    } else {
        // The size and density of the phone's screen, scaled to the window; some apps lay out well only with these
        arguments << QStringLiteral("--new-display");
    }
    arguments << QStringLiteral("--start-app=") + package;
    startWindow(package, i18nc("@title:window app name, phone name", "%1 - %2", name, m_phone->name()), arguments);
    return QString();
}

QString Scrcpy::showCamera(const QString &facing)
{
    const QString reason = unavailableReason();
    if (!reason.isEmpty()) {
        return reason;
    }
    const QString chosen = cameraFacing(facing);
    if (chosen.isEmpty()) {
        return i18nc("Keep \"back\" and \"front\" untranslated", "The camera is either \"back\" or \"front\".");
    } else if (m_webcam) {
        // Android gives the camera to only one of them
        return i18n("The phone camera is used as a webcam. Stop the webcam first.");
    }

    const QStringList arguments = cameraArguments(chosen);
    const QString title = chosen == QLatin1String("front") ? i18nc("@title:window phone name", "%1 Front Camera", m_phone->name())
                                                           : i18nc("@title:window phone name", "%1 Back Camera", m_phone->name());
    startWindow(QStringLiteral("camera"), title, arguments, true);
    return QString();
}

QString Scrcpy::cameraFacing(const QString &facing) const
{
    const QString chosen = facing.isEmpty() ? setting(QStringLiteral("camera")).toString() : facing;
    return chosen == QLatin1String("back") || chosen == QLatin1String("front") ? chosen : QString();
}

QStringList Scrcpy::cameraArguments(const QString &facing) const
{
    // Light enough for Wi-Fi; the cameras go up to 4096x3072
    QStringList arguments{QStringLiteral("--serial=") + m_adbSerial,
                          QStringLiteral("--video-source=camera"),
                          QStringLiteral("--camera-facing=") + facing,
                          QStringLiteral("--max-size=1920"),
                          QStringLiteral("--camera-fps=30")};
    // The picture is upright only for one way of holding the phone
    const int rotation = setting(QStringLiteral("cameraRotation")).toInt();
    if (rotation == 90 || rotation == 180 || rotation == 270) {
        arguments.append(QStringLiteral("--capture-orientation=%1").arg(rotation));
    }
    return arguments;
}

QString Scrcpy::startWebcam(const QString &facing)
{
    if (findWebcam()) {
        Q_EMIT statusChanged();
    }
    const QString reason = unavailableReason();
    const QString chosen = cameraFacing(facing);
    if (!reason.isEmpty()) {
        return reason;
    } else if (chosen.isEmpty()) {
        return i18nc("Keep \"back\" and \"front\" untranslated", "The camera is either \"back\" or \"front\".");
    } else if (m_webcam) {
        return chosen == m_webcamFacing ? QString() : i18n("The webcam already uses the other camera. Stop it first.");
    } else if (m_windows.contains(QStringLiteral("camera"))) {
        return i18n("The phone camera is shown in a window. Close it first.");
    } else if (m_webcamDevice.isEmpty()) {
        return i18n("No virtual webcam found. Install and load the v4l2loopback kernel module.");
    }

    // Visible while it runs, as nothing else shows that the camera is in use
    m_webcamNotification = new KNotification(QStringLiteral("phoneWebcam"), KNotification::Persistent);
    m_webcamNotification->setIconName(QStringLiteral("camera-web"));
    m_webcamNotification->setTitle(m_phone->name());
    m_webcamNotification->setText(chosen == QLatin1String("front") ? i18n("The front camera is used as the webcam \"%1\".", m_webcamName)
                                                                   : i18n("The back camera is used as the webcam \"%1\".", m_webcamName));
    connect(m_webcamNotification->addAction(i18n("Stop")), &KNotificationAction::activated, this, &Scrcpy::stopWebcam);
    // Dismissing it would leave the camera running unseen
    connect(m_webcamNotification, &KNotification::closed, this, &Scrcpy::stopWebcam);
    m_webcamNotification->sendEvent();

    m_webcamFacing = chosen;
    m_webcam = new QProcess(this);
    m_webcam->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_webcam, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
        const QStringList lines = QString::fromUtf8(m_webcam->readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        qCDebug(PHONEKOMPANIOND) << "scrcpy for the webcam ended with" << exitCode << (lines.isEmpty() ? QString() : lines.constLast());
        m_webcam->deleteLater();
        m_webcam = nullptr;
        if (m_webcamNotification) {
            m_webcamNotification->close();
        }
        Q_EMIT statusChanged();
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            return;
        }
        const QString error = scrcpyError(lines);
        showError(m_phone->name(), error.isEmpty() ? i18n("The phone webcam stopped unexpectedly.") : i18n("The phone webcam stopped: %1", error));
    });
    connect(m_webcam, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_webcam->deleteLater();
            m_webcam = nullptr;
            if (m_webcamNotification) {
                m_webcamNotification->close();
            }
            Q_EMIT statusChanged();
            showError(m_phone->name(), i18n("Could not start %1.", m_adb->scrcpy()));
        }
    });

    // No window: other apps show the picture
    QStringList arguments = cameraArguments(chosen);
    arguments << QStringLiteral("--no-audio") << QStringLiteral("--no-playback") << QStringLiteral("--v4l2-sink=") + m_webcamDevice;
    m_webcam->start(m_adb->scrcpy(), arguments);
    qCDebug(PHONEKOMPANIOND) << "Webcam on" << m_webcamDevice << chosen;

    Q_EMIT statusChanged();
    return QString();
}

void Scrcpy::stopWebcam()
{
    if (m_webcam) {
        m_webcam->terminate();
    }
}

void Scrcpy::startWindow(const QString &key, const QString &title, const QStringList &arguments, bool camera)
{
    if (QProcess *existing = m_windows.value(key)) {
        focusWindow(existing->processId());
        return;
    }

    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);
    m_windows.insert(key, process);
    connect(process, &QProcess::finished, this, [this, key, title, process](int exitCode, QProcess::ExitStatus exitStatus) {
        windowClosed(key);
        process->deleteLater();
        const QStringList lines = QString::fromUtf8(process->readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        qCDebug(PHONEKOMPANIOND) << "scrcpy for" << key << "ended with" << exitCode << (lines.isEmpty() ? QString() : lines.constLast());
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            return;
        }
        const QString error = scrcpyError(lines);
        showError(title, error.isEmpty() ? i18n("Stopped unexpectedly.") : i18n("Stopped: %1", error));
    });
    connect(process, &QProcess::errorOccurred, this, [this, key, title, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            qCWarning(PHONEKOMPANIOND) << "Could not start" << m_adb->scrcpy() << process->errorString();
            windowClosed(key);
            process->deleteLater();
            showError(title, i18n("Could not start %1.", m_adb->scrcpy()));
        }
    });

    QStringList allArguments = arguments;
    // Only one window plays the phone's sound, as each would capture all of it
    if (camera || !setting(QStringLiteral("audio")).toBool() || !m_audioWindow.isEmpty()) {
        allArguments << QStringLiteral("--no-audio");
    } else {
        m_audioWindow = key;
    }
    allArguments << QStringLiteral("--window-title=") + title;
    // Last, so that they can override the options above; the camera rejects many of them
    if (!camera) {
        allArguments << QProcess::splitCommand(setting(QStringLiteral("extraOptions")).toString());
    }
    qCDebug(PHONEKOMPANIOND) << "Starting scrcpy" << allArguments;
    process->start(m_adb->scrcpy(), allArguments);
}

void Scrcpy::windowClosed(const QString &key)
{
    m_windows.remove(key);
    if (m_audioWindow == key) {
        m_audioWindow.clear();
    }
}

void Scrcpy::showError(const QString &title, const QString &text)
{
    auto *notification = new KNotification(QStringLiteral("error"));
    notification->setTitle(title);
    notification->setText(text);
    notification->sendEvent();
}

void Scrcpy::focusWindow(qint64 pid)
{
    // On Wayland only the compositor can raise another program's window
    auto *script = new QTemporaryFile(this);
    if (pid <= 0 || !script->open()) {
        delete script;
        return;
    }
    script->write(QStringLiteral("for (const window of workspace.windowList()) {\n"
                                 "    if (window.pid === %1) {\n"
                                 "        window.minimized = false;\n"
                                 "        workspace.activeWindow = window;\n"
                                 "    }\n"
                                 "}\n")
                      .arg(pid)
                      .toUtf8());
    script->close();

    // One name per window; loading fails while a request for the same window still runs
    const QString name = KWIN_FOCUS_SCRIPT + QString::number(pid);
    QDBusMessage load = QDBusMessage::createMethodCall(KWIN_SERVICE, QStringLiteral("/Scripting"), KWIN_SCRIPTING_INTERFACE, QStringLiteral("loadScript"));
    load.setArguments({script->fileName(), name});
    auto *loadWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(load), this);
    connect(loadWatcher, &QDBusPendingCallWatcher::finished, this, [this, script, name](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<int> reply = *watcher;
        if (reply.isError() || reply.value() < 0) {
            qCDebug(PHONEKOMPANIOND) << "Could not bring the window to the front" << reply.error().message();
            script->deleteLater();
            return;
        }
        const QString path = QStringLiteral("/Scripting/Script%1").arg(reply.value());
        QDBusMessage run = QDBusMessage::createMethodCall(KWIN_SERVICE, path, QStringLiteral("org.kde.kwin.Script"), QStringLiteral("run"));
        auto *runWatcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(run), this);
        connect(runWatcher, &QDBusPendingCallWatcher::finished, this, [script, name](QDBusPendingCallWatcher *watcher) {
            watcher->deleteLater();
            QDBusMessage unload =
                QDBusMessage::createMethodCall(KWIN_SERVICE, QStringLiteral("/Scripting"), KWIN_SCRIPTING_INTERFACE, QStringLiteral("unloadScript"));
            unload.setArguments({name});
            QDBusConnection::sessionBus().asyncCall(unload);
            script->deleteLater();
        });
    });
}

QString Scrcpy::apps() const
{
    return m_state.readEntry(QStringLiteral("apps"), QStringLiteral("[]"));
}

QString Scrcpy::refreshApps()
{
    const QString reason = unavailableReason();
    if (m_appsRequest || !reason.isEmpty()) {
        return reason;
    }
    m_appsRequest = new QProcess(this);
    connect(m_appsRequest, &QProcess::finished, this, [this] {
        static const QRegularExpression appLine(QStringLiteral("^ [*-] (.*\\S)\\s+(\\S+)$"));
        const QStringList lines = QString::fromUtf8(m_appsRequest->readAllStandardOutput()).split(QLatin1Char('\n'));
        m_appsRequest->deleteLater();
        m_appsRequest = nullptr;

        // " * <name> <package>" for system apps, " - <name> <package>" for the others
        QList<std::pair<QString, QString>> found;
        for (const QString &line : lines) {
            const QRegularExpressionMatch match = appLine.match(line);
            if (match.hasMatch()) {
                found.append({match.captured(1), match.captured(2)});
            }
        }
        if (found.isEmpty()) {
            Q_EMIT appsChanged();
            return;
        }
        std::sort(found.begin(), found.end(), [](const std::pair<QString, QString> &a, const std::pair<QString, QString> &b) {
            return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
        });
        QJsonArray apps;
        for (const auto &[name, package] : std::as_const(found)) {
            apps.append(QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("package"), package}});
        }
        const QString json = QString::fromUtf8(QJsonDocument(apps).toJson(QJsonDocument::Compact));
        if (json != this->apps()) {
            m_state.writeEntry(QStringLiteral("apps"), json);
            m_state.sync();
        }
        Q_EMIT appsChanged();
        qCDebug(PHONEKOMPANIOND) << "Apps on" << m_phone->name() << apps.size();
    });
    connect(m_appsRequest, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_appsRequest->deleteLater();
            m_appsRequest = nullptr;
            Q_EMIT appsChanged();
        }
    });
    m_appsRequest->start(m_adb->scrcpy(), {QStringLiteral("--serial=") + m_adbSerial, QStringLiteral("--list-apps")});
    QTimer::singleShot(30000, m_appsRequest, &QProcess::kill);
    return QString();
}

QStringList Scrcpy::missingTools() const
{
    return m_adb->missingTools();
}

bool Scrcpy::networkReachable() const
{
    return m_networkReachable;
}

QStringList Scrcpy::adbDevices() const
{
    return m_adb->devices();
}

QString Scrcpy::adbSerial() const
{
    return m_adbSerial;
}

QString Scrcpy::webcamDevice() const
{
    return m_webcamDevice;
}

QString Scrcpy::webcamName() const
{
    return m_webcamName;
}

bool Scrcpy::webcamActive() const
{
    return m_webcam;
}

#include "moc_scrcpy.cpp"
