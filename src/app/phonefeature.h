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
#include <utility>

class QDBusMessage;

/**
 * A feature object of a phone in phonekompaniond, /phones/<id>/<name>, over D-Bus: its status
 * properties, its settings and its methods. All calls are asynchronous and do not start the
 * daemon; errors of the actions arrive as error().
 */
class PhoneFeature : public QObject
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(QString phoneId READ phoneId WRITE setPhoneId NOTIFY phoneIdChanged)
    // False until the status was read, and while the phone or the service is gone
    Q_PROPERTY(bool available READ isAvailable NOTIFY statusChanged)
    // Empty until read; see settings() of the daemon's object for the keys
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged)

public:
    PhoneFeature(const QString &name, const QString &interface, QObject *parent = nullptr);

    QString phoneId() const;
    void setPhoneId(const QString &phoneId);
    bool isAvailable() const;
    QVariantMap settings() const;

    Q_INVOKABLE void setSetting(const QString &key, const QVariant &value);

Q_SIGNALS:
    void phoneIdChanged();
    void statusChanged();
    void settingsChanged();
    void error(const QString &message);

protected:
    using SignalSlot = std::pair<const char *, const char *>;

    QVariant status(const QString &key) const;
    bool hasPhone() const;
    void call(const QString &method, const QVariantList &arguments, const std::function<void(const QDBusMessage &)> &callback = {});
    /**
     * Calls an action that answers with an error message, an empty string or nothing;
     * the error, or why the call failed, arrives as error()
     */
    void callAction(const QString &method, const QVariantList &arguments = {});

    /**
     * Reads what the subclass shows besides the status and the settings, for a new phone or daemon
     */
    virtual void readMore();
    /**
     * Forgets the state of the old phone or daemon; overrides call this one too
     */
    virtual void reset();
    /**
     * The daemon object's signals that change more than the status, and the slots for them
     */
    virtual QList<SignalSlot> signalSlots() const;
    /**
     * After the status was read; empty when the object is gone
     */
    virtual void statusRead();

protected Q_SLOTS:
    void readStatus();
    void readSettings();

private:
    QString path() const;
    void connectSignals(bool connect);
    void readAll();

    const QString m_name;
    const QString m_interface;
    QString m_phoneId;
    QVariantMap m_status;
    QVariantMap m_settings;
    // Replies for an older phone, daemon or status are dropped
    quint64 m_generation = 0;
    quint64 m_statusGeneration = 0;
};
