/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <KConfigGroup>
#include <KNotification>
#include <QDBusVariant>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QPointer>
#include <QStringList>

class Adb;
class Phone;
class QProcess;

/**
 * Mirrors the phone's screen, apps and camera with scrcpy, at /phones/<id>/scrcpy
 */
class Scrcpy : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.rkrajnc.phonekompanion.scrcpy")
    Q_PROPERTY(QStringList missingTools READ missingTools)
    // adb needs a network connection to the phone, Bluetooth is not enough
    Q_PROPERTY(bool networkReachable READ networkReachable)
    Q_PROPERTY(QStringList adbDevices READ adbDevices)
    // Empty while the phone's Wireless debugging is off
    Q_PROPERTY(QString adbSerial READ adbSerial)
    // A v4l2loopback device, for using the phone's camera as a webcam
    Q_PROPERTY(QString webcamDevice READ webcamDevice)
    Q_PROPERTY(QString webcamName READ webcamName)
    Q_PROPERTY(bool webcamActive READ webcamActive)
    // Why mirroring cannot start, empty when it can
    Q_PROPERTY(QString unavailableReason READ unavailableReason)

public:
    Scrcpy(Phone *phone, Adb *adb);
    ~Scrcpy() override;

    QStringList missingTools() const;
    bool networkReachable() const;
    QStringList adbDevices() const;
    QString adbSerial() const;
    QString webcamDevice() const;
    QString webcamName() const;
    bool webcamActive() const;
    QString unavailableReason() const;

    /**
     * The settings, with their defaults where not set: adbSerial, keyboard ("uhid" or empty),
     * turnScreenOff, audio, extraOptions, appDisplay ("phone", "portrait" or "landscape"),
     * appDensity, camera ("back" or "front"), cameraRotation (0, 90, 180 or 270)
     */
    Q_SCRIPTABLE QVariantMap settings() const;
    Q_SCRIPTABLE void setSetting(const QString &key, const QDBusVariant &value);

    /**
     * Pairs this computer with the phone, using the address and code from the phone's
     * "Pair device with pairing code" dialog. Answers with pairingFinished.
     */
    Q_SCRIPTABLE void pair(const QString &address, const QString &code);
    /**
     * Starts pairing with the phone's "Pair device with QR code" dialog. Returns the text to show
     * as a QR code, or an empty string if adb is not installed. Answers with pairingFinished once
     * the phone scanned the code, or after two minutes.
     */
    Q_SCRIPTABLE QString startQrPairing();
    /**
     * Stops waiting for the phone to scan the QR code
     */
    Q_SCRIPTABLE void stopQrPairing();

    /**
     * Shows the phone's screen in a window. Returns an error message, or an empty string if
     * scrcpy was started or its window was brought to the front.
     */
    Q_SCRIPTABLE QString mirrorScreen();
    /**
     * Opens an app on a virtual display of the phone, in its own window. Returns an error
     * message, or an empty string as mirrorScreen().
     */
    Q_SCRIPTABLE QString openApp(const QString &package);
    /**
     * Shows a camera of the phone in a window: facing "back", "front", or empty for the one
     * chosen in the settings. Returns an error message, or an empty string as mirrorScreen().
     */
    Q_SCRIPTABLE QString showCamera(const QString &facing);
    /**
     * Sends a camera of the phone to the v4l2loopback device, where other apps can use it as a
     * webcam. facing and the return value as for showCamera().
     */
    Q_SCRIPTABLE QString startWebcam(const QString &facing);
    /**
     * Stops using the phone camera as a webcam
     */
    Q_SCRIPTABLE void stopWebcam();

    /**
     * The phone's apps as a JSON array of {name, package}, from the last refreshApps()
     */
    Q_SCRIPTABLE QString apps() const;
    /**
     * Reads the phone's apps again and answers with appsChanged. Returns why that is not
     * possible, or an empty string.
     */
    Q_SCRIPTABLE QString refreshApps();

    /**
     * Packages of the apps with a launcher in the application menu
     */
    Q_SCRIPTABLE QStringList launchers() const;
    /**
     * Adds a launcher for the app to the application menu, or removes it. Returns an error
     * message, or an empty string.
     */
    Q_SCRIPTABLE QString setLauncher(const QString &package, bool enabled);
    void removeLaunchers();

    /**
     * The apps as from apps(), parsed
     */
    QJsonArray appList() const;

Q_SIGNALS:
    Q_SCRIPTABLE void statusChanged();
    Q_SCRIPTABLE void settingsChanged();
    Q_SCRIPTABLE void pairingFinished(bool success, const QString &message);
    /**
     * After refreshApps(), with why the apps could not be read, or an empty error
     */
    Q_SCRIPTABLE void appsChanged(const QString &error);
    Q_SCRIPTABLE void launchersChanged();

private:
    QVariant setting(const QString &key) const;
    /**
     * Returns whether the device changed
     */
    bool findWebcam();
    QString cameraFacing(const QString &facing) const;
    QStringList cameraArguments(const QString &facing) const;
    void matchDevice(bool devicesChanged = false);
    void runPair(const QString &address, const QString &code, const QString &retryMessage);
    void pollQrPairing();
    void endQrPairing();
    QStringList scrcpyArguments() const;
    void startWindow(const QString &key, const QString &title, const QStringList &arguments, bool camera = false);
    void windowClosed(const QString &key);
    QString launcherPath(const QString &package) const;
    QString writeLauncher(const QString &package);
    void focusWindow(qint64 pid);

    Phone *m_phone;
    Adb *m_adb;
    // The phone is gone when the destructor runs
    const QString m_dbusPath;
    KConfigGroup m_config;
    // Not settings: the app list is a cache
    KConfigGroup m_state;
    QString m_adbSerial;
    // Service name and password in the shown QR code, empty when none is shown
    QString m_qrName;
    QString m_qrPassword;
    QElapsedTimer m_qrTime;
    bool m_networkReachable = false;
    // "screen", "camera" or the app's package -> its scrcpy process
    QHash<QString, QProcess *> m_windows;
    // The window playing the phone's sound
    QString m_audioWindow;
    QProcess *m_appsRequest = nullptr;
    QJsonArray m_appList;
    // The phone's name in the launchers, rewritten when the phone is renamed
    QString m_launcherName;
    QString m_webcamDevice;
    QString m_webcamName;
    QProcess *m_webcam = nullptr;
    QString m_webcamFacing;
    QPointer<KNotification> m_webcamNotification;
};
