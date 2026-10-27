/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include "phonefeature.h"

/**
 * A phone's screen mirroring, /phones/<id>/scrcpy of phonekompaniond
 */
class ScrcpyInterface : public PhoneFeature
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QStringList missingTools READ missingTools NOTIFY statusChanged)
    Q_PROPERTY(bool networkReachable READ networkReachable NOTIFY statusChanged)
    Q_PROPERTY(QStringList adbDevices READ adbDevices NOTIFY statusChanged)
    Q_PROPERTY(QString adbSerial READ adbSerial NOTIFY statusChanged)
    Q_PROPERTY(QString webcamDevice READ webcamDevice NOTIFY statusChanged)
    Q_PROPERTY(QString webcamName READ webcamName NOTIFY statusChanged)
    Q_PROPERTY(bool webcamActive READ webcamActive NOTIFY statusChanged)
    // Why mirroring cannot start, empty when it can or while not available
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY statusChanged)
    // {name, package} objects, the cached list until refreshApps() answers
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(bool refreshingApps READ isRefreshingApps NOTIFY appsChanged)
    // Why refreshApps() could not read the apps
    Q_PROPERTY(QString appsError READ appsError NOTIFY appsChanged)
    // Packages of the apps with a launcher in the application menu
    Q_PROPERTY(QStringList launchers READ launchers NOTIFY launchersChanged)
    Q_PROPERTY(bool pairing READ isPairing NOTIFY pairingChanged)
    // The text to show as a QR code while the phone may scan it
    Q_PROPERTY(QString qrContent READ qrContent NOTIFY pairingChanged)

public:
    explicit ScrcpyInterface(QObject *parent = nullptr);

    QStringList missingTools() const;
    bool networkReachable() const;
    QStringList adbDevices() const;
    QString adbSerial() const;
    QString webcamDevice() const;
    QString webcamName() const;
    bool webcamActive() const;
    QString unavailableReason() const;
    QVariantList apps() const;
    bool isRefreshingApps() const;
    QString appsError() const;
    QStringList launchers() const;
    bool isPairing() const;
    QString qrContent() const;

    Q_INVOKABLE void mirrorScreen();
    Q_INVOKABLE void openApp(const QString &package);
    Q_INVOKABLE void showCamera(const QString &facing = QString());
    Q_INVOKABLE void startWebcam(const QString &facing = QString());
    Q_INVOKABLE void stopWebcam();
    Q_INVOKABLE void refreshApps();
    Q_INVOKABLE void setLauncher(const QString &package, bool enabled);
    Q_INVOKABLE void pair(const QString &address, const QString &code);
    Q_INVOKABLE void startQrPairing();
    Q_INVOKABLE void stopQrPairing();

Q_SIGNALS:
    void appsChanged();
    void launchersChanged();
    void pairingChanged();
    /**
     * Only for pairing started here
     */
    void pairingFinished(bool success, const QString &message);

protected:
    void readMore() override;
    void reset() override;
    QList<SignalSlot> signalSlots() const override;
    void statusRead() override;

private Q_SLOTS:
    void readApps();
    void readLaunchers();
    void onAppsChanged(const QString &error);
    void onPairingFinished(bool success, const QString &message);

private:
    QVariantList m_apps;
    bool m_refreshingApps = false;
    QString m_appsError;
    QStringList m_launchers;
    bool m_pairing = false;
    QString m_qrContent;
    // Waiting for startQrPairing() to answer, with the pairingFinished message meanwhile
    bool m_qrStarting = false;
    QString m_qrStartMessage;
};
