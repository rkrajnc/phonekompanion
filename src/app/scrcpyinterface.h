/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QVariantMap>

#include <functional>

class QDBusMessage;

/**
 * A phone's /phones/<id>/scrcpy object of phonekompaniond, over D-Bus. All calls are
 * asynchronous; errors of the actions arrive as error().
 */
class ScrcpyInterface : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString phoneId READ phoneId WRITE setPhoneId NOTIFY phoneIdChanged)
    // False until the status was read, and while the phone or the service is gone
    Q_PROPERTY(bool available READ isAvailable NOTIFY statusChanged)
    Q_PROPERTY(QStringList missingTools READ missingTools NOTIFY statusChanged)
    Q_PROPERTY(bool networkReachable READ networkReachable NOTIFY statusChanged)
    Q_PROPERTY(QStringList adbDevices READ adbDevices NOTIFY statusChanged)
    Q_PROPERTY(QString adbSerial READ adbSerial NOTIFY statusChanged)
    Q_PROPERTY(QString webcamDevice READ webcamDevice NOTIFY statusChanged)
    Q_PROPERTY(QString webcamName READ webcamName NOTIFY statusChanged)
    Q_PROPERTY(bool webcamActive READ webcamActive NOTIFY statusChanged)
    // Why mirroring cannot start, empty when it can or while not available
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY statusChanged)
    // Empty until read; see Scrcpy::settings() in the daemon for the keys
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)
    // {name, package} objects, the cached list until refreshApps() answers
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)
    Q_PROPERTY(bool refreshingApps READ isRefreshingApps NOTIFY appsChanged)
    // Why refreshApps() could not read the apps
    Q_PROPERTY(QString appsError READ appsError NOTIFY appsChanged)
    Q_PROPERTY(bool pairing READ isPairing NOTIFY pairingChanged)
    // The text to show as a QR code while the phone may scan it
    Q_PROPERTY(QString qrContent READ qrContent NOTIFY pairingChanged)

public:
    explicit ScrcpyInterface(QObject *parent = nullptr);

    QString phoneId() const;
    void setPhoneId(const QString &phoneId);

    bool isAvailable() const;
    QStringList missingTools() const;
    bool networkReachable() const;
    QStringList adbDevices() const;
    QString adbSerial() const;
    QString webcamDevice() const;
    QString webcamName() const;
    bool webcamActive() const;
    QString unavailableReason() const;
    QVariantMap settings() const;
    QVariantList apps() const;
    bool isRefreshingApps() const;
    QString appsError() const;
    bool isPairing() const;
    QString qrContent() const;

    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);
    Q_INVOKABLE void mirrorScreen();
    Q_INVOKABLE void openApp(const QString &package);
    Q_INVOKABLE void showCamera(const QString &facing = QString());
    Q_INVOKABLE void startWebcam(const QString &facing = QString());
    Q_INVOKABLE void stopWebcam();
    Q_INVOKABLE void refreshApps();
    Q_INVOKABLE void pair(const QString &address, const QString &code);
    Q_INVOKABLE void startQrPairing();
    Q_INVOKABLE void stopQrPairing();

Q_SIGNALS:
    void phoneIdChanged();
    void statusChanged();
    void settingsChanged();
    void appsChanged();
    void pairingChanged();
    void error(const QString &message);
    /**
     * Only for pairing started here
     */
    void pairingFinished(bool success, const QString &message);

private Q_SLOTS:
    void readStatus();
    void readSettings();
    void readApps();
    void onAppsChanged();
    void onPairingFinished(bool success, const QString &message);

private:
    QString path() const;
    void connectSignals(bool connect);
    void call(const QString &method, const QVariantList &arguments, const std::function<void(const QDBusMessage &)> &callback = {});
    /**
     * Calls an action that answers with an error message, or an empty string
     */
    void callAction(const QString &method, const QVariantList &arguments = {});
    void reset();

    QString m_phoneId;
    QVariantMap m_status;
    QVariantMap m_settings;
    QVariantList m_apps;
    bool m_refreshingApps = false;
    QString m_appsError;
    bool m_pairing = false;
    QString m_qrContent;
    // Waiting for startQrPairing() to answer, with the pairingFinished message meanwhile
    bool m_qrStarting = false;
    QString m_qrStartMessage;
    // Replies for an older phone or status are dropped
    quint64 m_generation = 0;
    quint64 m_statusGeneration = 0;
};
