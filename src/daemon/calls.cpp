/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "calls.h"

#include <KLocalizedString>
#include <KPeopleBackend/AbstractContact>
#include <KSharedConfig>
#include <QDBusArgument>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QRegularExpression>
#include <QTimer>

#include "config-phonekompaniond.h"
#include "phone.h"
#include "phonekompaniond_debug.h"
#include "phonenumbermatch.h"
#include "settings.h"

#if HAVE_PULSEAUDIO
#include <PulseAudioQt/Context>
#include <PulseAudioQt/SourceOutput>
#endif

#include <algorithm>

#define TELEPHONY_SERVICE QStringLiteral("org.pipewire.Telephony")
#define GATEWAY_INTERFACE QStringLiteral("org.pipewire.Telephony.AudioGateway1")
#define TRANSPORT_INTERFACE QStringLiteral("org.pipewire.Telephony.AudioGatewayTransport1")
#define CALL_INTERFACE QStringLiteral("org.pipewire.Telephony.Call1")
#define OBJECT_MANAGER_INTERFACE QStringLiteral("org.freedesktop.DBus.ObjectManager")
#define PROPERTIES_INTERFACE QStringLiteral("org.freedesktop.DBus.Properties")
#define BLUEZ_SERVICE QStringLiteral("org.bluez")
#define BLUEZ_DEVICE_INTERFACE QStringLiteral("org.bluez.Device1")
#define HFP_AG_UUID QStringLiteral("0000111f-0000-1000-8000-00805f9b34fb")

namespace
{
#if HAVE_PULSEAUDIO
// The capture stream carrying this computer's microphone to the phone
void applyMute(PulseAudioQt::SourceOutput *output, const QString &address, bool muted)
{
    const QString streamAddress = output->properties().value(QStringLiteral("api.bluez5.address")).toString();
    if (!address.isEmpty() && streamAddress.compare(address, Qt::CaseInsensitive) == 0) {
        output->setMuted(muted);
    }
}
#endif

const QVariantMap &defaultSettings()
{
    static const QVariantMap defaults{
        {QStringLiteral("useThisComputer"), true},
        {QStringLiteral("autoConnect"), true},
        {QStringLiteral("gatewayAddress"), QString()},
    };
    return defaults;
}
}

Calls::Calls(Phone *phone, Bluetooth *bluetooth)
    : QObject(phone)
    , m_phone(phone)
    , m_bluetooth(bluetooth)
    , m_dbusPath(phone->dbusPath() + QStringLiteral("/calls"))
    , m_config(KSharedConfig::openConfig(QStringLiteral("phonekompanionrc"))->group(phone->id()).group(QStringLiteral("Calls")))
{
    m_useThisComputer = setting(QStringLiteral("useThisComputer")).toBool();
    connect(m_phone, &Phone::changed, this, &Calls::phoneChanged);
    connect(m_bluetooth, &Bluetooth::changed, this, &Calls::bluetoothChanged);
    connect(m_bluetooth, &Bluetooth::lockedChanged, this, &Calls::refreshNotifications);
#if HAVE_PULSEAUDIO
    // Recreated whenever the call audio moves here, with the mute state WirePlumber saved last
    auto *context = PulseAudioQt::Context::instance();
    connect(context, &PulseAudioQt::Context::sourceOutputAdded, this, [this](PulseAudioQt::SourceOutput *output) {
        applyMute(output, m_gatewayAddress, m_microphoneMuted);
        connect(output, &PulseAudioQt::SourceOutput::mutedChanged, this, [this, output] {
            if (output->isMuted() != m_microphoneMuted) {
                applyMute(output, m_gatewayAddress, m_microphoneMuted);
            }
        });
    });
    connect(context, &PulseAudioQt::Context::stateChanged, this, [this] {
        applyMicrophoneMute();
        refreshNotifications();
        Q_EMIT statusChanged();
    });
#endif

    bluetoothChanged();

    QDBusConnection::sessionBus().registerObject(m_dbusPath, this, QDBusConnection::ExportScriptableContents);
}

Calls::~Calls()
{
    QDBusConnection::sessionBus().unregisterObject(m_dbusPath);
    for (const Call &call : std::as_const(m_calls)) {
        closeNotifications(call);
    }
    if (m_telephonyNotification) {
        m_telephonyNotification->close();
    }
    restoreRejectSco(true);
    // The call goes on without us, e.g. when the phone is unpaired or the service quits
    if (m_microphoneMuted) {
        m_microphoneMuted = false;
        applyMicrophoneMute();
    }
}

void Calls::telephonyCallReceived(const QString &event, const QString &number, const QString &contactName)
{
    qCDebug(PHONEKOMPANIOND) << "Telephony signal, event" << event << "gateway connected" << gatewayConnected();

    // With a Bluetooth gateway used for calls, the HFP calls drive the notifications
    if (gatewayConnected()) {
        updateCallerFromTelephony(number, contactName);
        return;
    }

    const QString caller = contactName.isEmpty() ? number : contactName;
    if (event == QLatin1String("missedCall")) {
        if (m_telephonyNotification) {
            m_telephonyNotification->close();
        }
        // Already shown from HFP, when "Use this computer" was turned off during the call
        if (!m_hfpMissedCall.isValid() || m_hfpMissedCall.elapsed() > 10000) {
            showMissedCall(caller, QString());
        }
    } else if (event == QLatin1String("callReceived")) {
        // KDE Connect does not tell when the call is answered or ends, so it closes by itself
        const bool created = !m_telephonyNotification;
        if (created) {
            m_telephonyNotification = new KNotification(QStringLiteral("callReceived"));
            m_telephonyNotification->setTitle(m_phone->name());
            m_telephonyNotification->setIconName(QStringLiteral("call-start"));
        }
        m_telephonyNotification->setText(i18n("Incoming call from %1", caller));
        if (created) {
            m_telephonyNotification->sendEvent();
        }
    }
}

void Calls::autoConnect()
{
    // Decided once each time the phone comes within KDE Connect's reach, once both BlueZ and PipeWire have answered
    if (m_autoConnectDecided || !m_reachable || !m_bluetooth->pairedPhonesLoaded() || !m_bluetooth->telephonyAvailable()) {
        return;
    }
    m_autoConnectDecided = true;
    const QString address = gatewayAddress();
    if (wantsConnection(address)) {
        connectPhone(address, true);
    }
}

bool Calls::wantsConnection(const QString &address) const
{
    return m_useThisComputer && setting(QStringLiteral("autoConnect")).toBool() && !address.isEmpty() && address == gatewayAddress()
        && m_bluetooth->gateways().key(address).isEmpty() && !m_bluetooth->phoneObject(address).isEmpty();
}

void Calls::connectPhone(const QString &address, bool retry)
{
    qCDebug(PHONEKOMPANIOND) << "Connecting Bluetooth phone" << address;
    QDBusMessage request =
        QDBusMessage::createMethodCall(BLUEZ_SERVICE, m_bluetooth->phoneObject(address), BLUEZ_DEVICE_INTERFACE, QStringLiteral("ConnectProfile"));
    request.setArguments({HFP_AG_UUID});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, address, retry](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (!watcher->isError()) {
            return;
        }
        const QDBusError error = watcher->error();
        qCDebug(PHONEKOMPANIOND) << "Connecting" << address << "failed:" << error.name() << error.message();
        // One more try for transient failures such as br-connection-busy, if it is still wanted then
        const bool settled = error.name() == QLatin1String("org.bluez.Error.AlreadyConnected") || error.name() == QLatin1String("org.bluez.Error.InProgress");
        if (retry && !settled) {
            QTimer::singleShot(3000, this, [this, address] {
                if (wantsConnection(address)) {
                    connectPhone(address, false);
                }
            });
        }
    });
}

QVariant Calls::setting(const QString &key) const
{
    return m_config.readEntry(key, defaultSettings().value(key));
}

QVariantMap Calls::settings() const
{
    QVariantMap settings;
    for (auto it = defaultSettings().cbegin(); it != defaultSettings().cend(); ++it) {
        settings.insert(it.key(), setting(it.key()));
    }
    return settings;
}

void Calls::setSetting(const QString &key, const QDBusVariant &value)
{
    QVariant converted = value.variant();
    if (!defaultSettings().contains(key) || !convertSetting(converted, defaultSettings().value(key))) {
        qCWarning(PHONEKOMPANIOND) << "Ignoring the setting" << key << value.variant();
        return;
    }
    m_config.writeEntry(key, converted);
    m_config.sync();
    Q_EMIT settingsChanged();
    resolveGateway();
    useThisComputerChanged();
}

void Calls::phoneChanged()
{
    // The gateway may be found by the phone's name
    if (m_phone->name() != m_name) {
        m_name = m_phone->name();
        resolveGateway();
    }
    // Back within KDE Connect's reach, e.g. home again: connect Bluetooth as well
    if (m_phone->isReachable() != m_reachable) {
        m_reachable = m_phone->isReachable();
        if (m_reachable) {
            m_autoConnectDecided = false;
            autoConnect();
        }
    }
}

void Calls::bluetoothChanged()
{
    Q_EMIT statusChanged();
    resolveGateway();
    autoConnect();
}

bool Calls::telephonyAvailable() const
{
    return m_bluetooth->telephonyAvailable();
}

QVariantMap Calls::pairedPhones() const
{
    QVariantMap phones;
    const QHash<QString, QString> pairedPhones = m_bluetooth->pairedPhones();
    for (auto it = pairedPhones.cbegin(); it != pairedPhones.cend(); ++it) {
        phones.insert(it.key(), it.value());
    }
    return phones;
}

QStringList Calls::connectedPhones() const
{
    return m_bluetooth->gateways().values();
}

QString Calls::gatewayAddress() const
{
    const QString configured = setting(QStringLiteral("gatewayAddress")).toString().toUpper();
    if (!configured.isEmpty()) {
        return configured;
    }

    const QStringList sameName = m_bluetooth->pairedPhones().keys(m_phone->name());
    if (sameName.size() == 1) {
        return sameName.constFirst();
    }

    // Only for the only phone, as the gateway may belong to another one
    const QHash<QString, QString> gateways = m_bluetooth->gateways();
    if (gateways.size() == 1 && m_bluetooth->phoneCount() == 1) {
        return gateways.cbegin().value();
    }
    return QString();
}

void Calls::resolveGateway()
{
    const QString address = gatewayAddress();
    const QString path = address.isEmpty() ? QString() : m_bluetooth->gateways().key(address);
    if (path != m_gatewayPath) {
        selectGateway(path);
    }
}

void Calls::selectGateway(const QString &path)
{
    if (!m_gatewayPath.isEmpty()) {
        restoreRejectSco(false);
        watchChildren(m_gatewayPath, false);
        watchProperties(m_gatewayPath, false);
        for (auto it = m_calls.cbegin(); it != m_calls.cend(); ++it) {
            watchProperties(it.key(), false);
            closeNotifications(*it);
        }
        m_calls.clear();
    }

    if (m_microphoneMuted) {
        m_microphoneMuted = false;
        applyMicrophoneMute();
    }
    const bool wasConnected = gatewayConnected();
    m_gatewayPath = path;
    m_gatewayAddress = m_bluetooth->gateways().value(path);
    // Also clears a mute left on a stream that existed before
    applyMicrophoneMute();
    m_audioOnPc = false;
    m_pendingTones.clear();
    notifyCallChanged();
    if (gatewayConnected() != wasConnected) {
        Q_EMIT gatewayConnectedChanged(gatewayConnected());
    }
    if (path.isEmpty()) {
        qCDebug(PHONEKOMPANIOND) << "No Bluetooth gateway for" << m_phone->name();
        return;
    }

    qCDebug(PHONEKOMPANIOND) << "Using Bluetooth gateway" << m_gatewayAddress << "at" << path << "for" << m_phone->name();
    if (m_telephonyNotification) {
        m_telephonyNotification->close();
    }
    watchChildren(path, true);
    watchProperties(path, true);
    QDBusMessage request = QDBusMessage::createMethodCall(TELEPHONY_SERVICE, path, PROPERTIES_INTERFACE, QStringLiteral("GetAll"));
    request.setArguments({TRANSPORT_INTERFACE});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *watcher;
        if (!reply.isError() && path == m_gatewayPath) {
            m_audioOnPc = reply.value().value(QStringLiteral("State")).toString() == QLatin1String("active");
            refreshNotifications();
            notifyCallChanged();
        }
    });
    // Calls stay on the phone unless answered here
    setRejectSco(true);
    Bluetooth::getManagedObjects(QDBusConnection::sessionBus(), TELEPHONY_SERVICE, path, this, [this, path](const ObjectMap &objects) {
        if (path != m_gatewayPath) {
            return;
        }
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            addCall(it.key().path(), it.value());
        }
    });
}

void Calls::addCall(const QString &path, const InterfaceMap &interfaces)
{
    if (!interfaces.contains(CALL_INTERFACE) || m_calls.contains(path)) {
        return;
    }
    Call &call = m_calls[path];
    call.properties = interfaces.value(CALL_INTERFACE);
    call.handledHere = m_useThisComputer;
    resolveCaller(call);
    applyTelephonyCaller(call);
    watchProperties(path, true);
    qCDebug(PHONEKOMPANIOND) << "Call added" << path << "state" << call.properties.value(QStringLiteral("State")).toString();
    updateNotification(path, QString());
    notifyCallChanged();
}

void Calls::removeCall(const QString &path, const QStringList &interfaces)
{
    if (!interfaces.contains(CALL_INTERFACE) || !m_calls.contains(path)) {
        return;
    }
    closeNotifications(m_calls.take(path));
    watchProperties(path, false);
    // A call still ringing then stays on the phone unless answered here, like a new one
    if (std::all_of(m_calls.cbegin(), m_calls.cend(), [](const Call &call) {
            const QString state = call.properties.value(QStringLiteral("State")).toString();
            return state == QLatin1String("incoming") || state == QLatin1String("waiting");
        })) {
        setRejectSco(true);
    }
    if (m_calls.isEmpty() && m_microphoneMuted) {
        m_microphoneMuted = false;
        applyMicrophoneMute();
    }
    qCDebug(PHONEKOMPANIOND) << "Call removed" << path;
    notifyCallChanged();
}

void Calls::resolveCaller(Call &call) const
{
    call.name.clear();
    call.photo = QPixmap();
    const QString number = call.properties.value(QStringLiteral("LineIdentification")).toString();
    if (number.isEmpty()) {
        return;
    }
    const PhoneNumberMatcher matcher(number);
    const KPeople::PersonsModel &persons = m_bluetooth->persons();
    for (int row = 0; row < persons.rowCount(); ++row) {
        const QModelIndex person = persons.index(row);
        const auto contact = person.data(KPeople::PersonsModel::PersonVCardRole).value<KPeople::AbstractContact::Ptr>();
        if (!contact) {
            continue;
        }
        const QVariantList contactNumbers = contact->customProperty(KPeople::AbstractContact::AllPhoneNumbersProperty).toList();
        for (const QVariant &contactNumber : contactNumbers) {
            if (matcher.matches(contactNumber.toString())) {
                call.name = person.data(KPeople::PersonsModel::FormattedNameRole).toString();
                call.photo = person.data(KPeople::PersonsModel::PhotoRole).value<QPixmap>();
                qCDebug(PHONEKOMPANIOND) << "Caller found in contacts";
                return;
            }
        }
    }
}

void Calls::applyTelephonyCaller(Call &call) const
{
    if (!call.name.isEmpty() || !m_telephonyTime.isValid() || m_telephonyTime.elapsed() > 60000) {
        return;
    }
    if (PhoneNumberMatcher(m_telephonyNumber).matches(call.properties.value(QStringLiteral("LineIdentification")).toString())) {
        call.name = m_telephonyName;
        qCDebug(PHONEKOMPANIOND) << "Caller name from KDE Connect, sent before the call appeared";
    }
}

void Calls::updateCallerFromTelephony(const QString &number, const QString &contactName)
{
    if (number.isEmpty() || contactName.isEmpty() || contactName == number) {
        return;
    }
    m_telephonyNumber = number;
    m_telephonyName = contactName;
    m_telephonyTime.start();

    const PhoneNumberMatcher matcher(number);
    for (auto it = m_calls.begin(); it != m_calls.end(); ++it) {
        Call &call = it.value();
        if (!call.name.isEmpty() || !matcher.matches(call.properties.value(QStringLiteral("LineIdentification")).toString())) {
            continue;
        }
        call.name = contactName;
        notifyCallChanged();
        qCDebug(PHONEKOMPANIOND) << "Caller name from KDE Connect";
        const QString state = call.properties.value(QStringLiteral("State")).toString();
        updateNotification(it.key(), state);
    }
}

void Calls::updateNotification(const QString &path, const QString &previousState)
{
    Call &call = m_calls[path];
    if (!call.handledHere) {
        return;
    }
    const QString state = call.properties.value(QStringLiteral("State")).toString();
    QString number = call.properties.value(QStringLiteral("LineIdentification")).toString();
    if (number.isEmpty()) {
        number = i18n("unknown number");
    }
    const QString caller = call.name.isEmpty() ? number : call.name;

    const bool ringing = state == QLatin1String("incoming") || state == QLatin1String("waiting");
    const bool wasRinging = previousState == QLatin1String("incoming") || previousState == QLatin1String("waiting");

    if (state != QLatin1String("incoming") && call.ringtone) {
        call.ringtone->close();
    }

    if (state == QLatin1String("disconnected")) {
        closeNotifications(call);
        // Without +CLCC, PipeWire ends a waiting call answered with AT+CHLD instead of making it active
        if (wasRinging && !call.declined && !call.answered) {
            m_hfpMissedCall.start();
            showMissedCall(caller, call.properties.value(QStringLiteral("LineIdentification")).toString());
        }
        return;
    }

    const bool created = previousState.isEmpty();
    if (created) {
        call.notification = new KNotification(QStringLiteral("callReceived"), KNotification::Persistent);
        call.notification->setIconName(QStringLiteral("call-start"));
        call.notification->setTitle(m_phone->name());
        // Keep the notification open after Answer, so it can offer Hang Up
        call.notification->setHint(QStringLiteral("resident"), true);
    } else if (!call.notification) {
        return;
    }

    KNotification *notification = call.notification;
    // Also when a waiting call becomes incoming because the other call ended
    if (state == QLatin1String("incoming") && previousState != QLatin1String("incoming")) {
        call.ringtone = new KNotification(QStringLiteral("callRinging"), KNotification::Persistent | KNotification::LoopSound);
        connect(notification, &KNotification::closed, call.ringtone, &KNotification::close);
        call.ringtone->sendEvent();
    }
    if (!call.photo.isNull()) {
        notification->setPixmap(call.photo);
    }
    if (ringing && !call.name.isEmpty()) {
        notification->setText(i18n("Incoming call from %1 (%2)", call.name, number));
    } else if (ringing) {
        notification->setText(i18n("Incoming call from %1", caller));
    } else if (state == QLatin1String("dialing") || state == QLatin1String("alerting")) {
        notification->setText(i18n("Calling %1", caller));
    } else if (state == QLatin1String("held")) {
        notification->setText(i18n("Call with %1 (on hold)", caller));
    } else if (state == QLatin1String("active") && m_audioOnPc && m_microphoneMuted) {
        notification->setText(i18n("Call with %1 (microphone muted)", caller));
    } else if (m_audioOnPc) {
        notification->setText(i18n("Call with %1", caller));
    } else {
        notification->setText(i18n("Call with %1 (on phone)", caller));
    }

    // No answering and no moving audio to the PC from a locked session
    Actions actions;
    if (state == QLatin1String("incoming")) {
        actions = m_bluetooth->isLocked() ? Actions::RingingLocked : Actions::Ringing;
    } else if (state == QLatin1String("waiting")) {
        actions = m_bluetooth->isLocked() ? Actions::WaitingLocked : Actions::Waiting;
    } else if (state == QLatin1String("held")) {
        actions = Actions::Held;
    } else if (state == QLatin1String("active") && m_audioOnPc && microphoneMuteSupported()) {
        actions = m_microphoneMuted ? Actions::CallMuted : Actions::CallOnPc;
    } else {
        actions = m_audioOnPc || m_bluetooth->isLocked() ? Actions::Call : Actions::CallOnPhone;
    }

    if (actions != call.actions) {
        call.actions = actions;
        notification->clearActions();
        if (actions == Actions::Ringing || actions == Actions::Waiting) {
            connect(notification->addAction(i18n("Answer")), &KNotificationAction::activated, this, [this, path] {
                answerCall(path);
            });
        }
        if (ringing) {
            connect(notification->addAction(i18n("Decline")), &KNotificationAction::activated, this, [this, path] {
                declineCall(path);
            });
        }
        if (!ringing) {
            if (actions == Actions::CallOnPhone) {
                connect(notification->addAction(i18n("Move Audio to PC")), &KNotificationAction::activated, this, &Calls::moveAudioToPc);
            } else if (actions == Actions::CallOnPc || actions == Actions::CallMuted) {
                const bool mute = actions == Actions::CallOnPc;
                KNotificationAction *muteAction = notification->addAction(mute ? i18n("Mute Microphone") : i18n("Unmute Microphone"));
                connect(muteAction, &KNotificationAction::activated, this, [this, mute] {
                    setMicrophoneMuted(mute);
                });
            }
            connect(notification->addAction(i18n("Hang Up")), &KNotificationAction::activated, this, [this, path] {
                callMethod(path, CALL_INTERFACE, QStringLiteral("Hangup"));
            });
        }
    }

    if (created) {
        notification->sendEvent();
    }
}

void Calls::showMissedCall(const QString &caller, const QString &number)
{
    auto *notification = new KNotification(QStringLiteral("missedCall"), KNotification::Persistent);
    notification->setIconName(QStringLiteral("call-start"));
    notification->setTitle(m_phone->name());
    notification->setText(i18n("Missed call from %1", caller));
    if (!number.isEmpty() && gatewayConnected()) {
        connect(notification->addAction(i18n("Call Back")), &KNotificationAction::activated, this, [this, number] {
            dialNumber(number);
        });
    }
    notification->sendEvent();
}

QString Calls::currentCall() const
{
    const QList<QStringList> priorities = {
        {QStringLiteral("incoming"), QStringLiteral("waiting")},
        {QStringLiteral("dialing"), QStringLiteral("alerting")},
        {QStringLiteral("active")},
        {QStringLiteral("held")},
    };
    for (const QStringList &states : priorities) {
        for (auto it = m_calls.cbegin(); it != m_calls.cend(); ++it) {
            if (it->handledHere && states.contains(it->properties.value(QStringLiteral("State")).toString())) {
                return it.key();
            }
        }
    }
    return QString();
}

void Calls::notifyCallChanged()
{
    const QStringList info{callState(),
                           callName(),
                           callNumber(),
                           audioOnPc() ? QStringLiteral("pc") : QString(),
                           microphoneMuted() ? QStringLiteral("muted") : QString()};
    if (info != m_lastCallInfo) {
        m_lastCallInfo = info;
        Q_EMIT callChanged();
    }
}

QString Calls::callState() const
{
    const QString path = currentCall();
    return path.isEmpty() ? QString() : m_calls.value(path).properties.value(QStringLiteral("State")).toString();
}

QString Calls::callName() const
{
    return m_calls.value(currentCall()).name;
}

QString Calls::callNumber() const
{
    return m_calls.value(currentCall()).properties.value(QStringLiteral("LineIdentification")).toString();
}

bool Calls::audioOnPc() const
{
    return m_audioOnPc;
}

bool Calls::microphoneMuted() const
{
    return m_microphoneMuted;
}

bool Calls::microphoneMuteSupported() const
{
#if HAVE_PULSEAUDIO
    return PulseAudioQt::Context::instance()->state() == PulseAudioQt::Context::State::Ready;
#else
    return false;
#endif
}

void Calls::setMicrophoneMuted(bool muted)
{
    if (!microphoneMuteSupported() || muted == m_microphoneMuted
        || (muted && (!gatewayConnected() || !m_audioOnPc || callState() != QLatin1String("active")))) {
        return;
    }
    m_microphoneMuted = muted;
    applyMicrophoneMute();
    refreshNotifications();
    notifyCallChanged();
}

void Calls::applyMicrophoneMute()
{
#if HAVE_PULSEAUDIO
    const auto outputs = PulseAudioQt::Context::instance()->sourceOutputs();
    for (PulseAudioQt::SourceOutput *output : outputs) {
        applyMute(output, m_gatewayAddress, m_microphoneMuted);
    }
#endif
}

void Calls::answer()
{
    const QString path = currentCall();
    const QString state = callState();
    if (!path.isEmpty() && !m_bluetooth->isLocked() && (state == QLatin1String("incoming") || state == QLatin1String("waiting"))) {
        answerCall(path);
    }
}

void Calls::hangUp()
{
    const QString path = currentCall();
    const QString state = callState();
    if (path.isEmpty()) {
        return;
    } else if (state == QLatin1String("incoming") || state == QLatin1String("waiting")) {
        declineCall(path);
    } else {
        callMethod(path, CALL_INTERFACE, QStringLiteral("Hangup"));
    }
}

void Calls::sendTones(const QString &tones)
{
    static const QRegularExpression valid(QStringLiteral("^[0-9*#A-D]+$"));
    if (gatewayConnected() && callState() == QLatin1String("active") && valid.match(tones).hasMatch() && m_pendingTones.size() + tones.size() <= 32) {
        m_pendingTones += tones;
        if (!m_sendingTone) {
            sendNextTone();
        }
    }
}

void Calls::sendNextTone()
{
    // Checked before each tone, as the call may have ended or been put on hold
    if (m_pendingTones.isEmpty() || !gatewayConnected() || callState() != QLatin1String("active")) {
        m_pendingTones.clear();
        m_sendingTone = false;
        return;
    }
    const QString tone = m_pendingTones.left(1);
    m_pendingTones.remove(0, 1);
    m_sendingTone = true;
    QDBusMessage request = QDBusMessage::createMethodCall(TELEPHONY_SERVICE, m_gatewayPath, GATEWAY_INTERFACE, QStringLiteral("SendTones"));
    request.setArguments({tone});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (watcher->isError()) {
            qCWarning(PHONEKOMPANIOND) << "SendTones failed" << watcher->error().message();
            m_pendingTones.clear();
            m_sendingTone = false;
            return;
        }
        // A pause between tones, as the phone answers each one at once and they would run together
        QTimer::singleShot(250, this, &Calls::sendNextTone);
    });
}

QString Calls::searchContacts(const QString &query) const
{
    QJsonArray entries;
    const QString trimmed = query.trimmed();
    if (!trimmed.isEmpty() && !isPhoneNumber(trimmed)) {
        const QList<Contact> contacts = findContacts(trimmed, 20);
        for (const Contact &contact : contacts) {
            for (const QString &number : contact.numbers) {
                entries.append(QJsonObject{{QStringLiteral("name"), contact.name}, {QStringLiteral("number"), number}});
            }
        }
    }
    return QString::fromUtf8(QJsonDocument(entries).toJson(QJsonDocument::Compact));
}

bool Calls::gatewayConnected() const
{
    return m_useThisComputer && !m_gatewayPath.isEmpty();
}

void Calls::useThisComputerChanged()
{
    const bool use = setting(QStringLiteral("useThisComputer")).toBool();
    if (use == m_useThisComputer) {
        return;
    }
    const bool wasConnected = gatewayConnected();
    m_useThisComputer = use;
    // Turned off, calls already shown here keep their notification; turned on, running calls are shown too
    if (use) {
        if (m_telephonyNotification) {
            m_telephonyNotification->close();
        }
        const QStringList paths = m_calls.keys();
        for (const QString &path : paths) {
            Call &call = m_calls[path];
            if (!call.handledHere) {
                call.handledHere = true;
                call.actions = Actions::None;
                updateNotification(path, QString());
            }
        }
        m_autoConnectDecided = false;
        autoConnect();
    }
    notifyCallChanged();
    if (gatewayConnected() != wasConnected) {
        Q_EMIT gatewayConnectedChanged(gatewayConnected());
    }
}

QString Calls::dial(const QString &numberOrName)
{
    const QString query = numberOrName.trimmed();
    if (query.isEmpty()) {
        return i18n("No phone number or contact given");
    } else if (!m_useThisComputer) {
        return i18n("Calls are turned off for this computer in the Bluetooth calls settings");
    } else if (!gatewayConnected()) {
        return i18n("%1 is not connected over Bluetooth", m_phone->name());
    } else if (!m_calls.isEmpty()) {
        return i18n("Already in a call");
    }

    if (isPhoneNumber(query)) {
        dialNumber(query);
        return QString();
    } else if (!m_bluetooth->persons().isInitialized()) {
        return i18n("Contacts are still loading, try again in a moment");
    }

    // One more than shown, to know whether to add "..."
    const QList<Contact> contacts = findContacts(query, 11);
    if (contacts.isEmpty()) {
        return i18n("No contact matches %1", query);
    } else if (contacts.size() > 1) {
        QStringList names;
        for (const Contact &contact : contacts) {
            names.append(contact.name);
        }
        QString list = names.mid(0, 10).join(QStringLiteral(", "));
        if (names.size() > 10) {
            list += QStringLiteral(", ...");
        }
        return i18n("Several contacts match %1: %2", query, list);
    }

    const Contact &contact = contacts.constFirst();
    if (contact.numbers.isEmpty()) {
        return i18n("%1 has no phone number", contact.name);
    } else if (contact.numbers.size() > 1) {
        return i18n("%1 has several phone numbers: %2", contact.name, contact.numbers.join(QStringLiteral(", ")));
    }
    dialNumber(contact.numbers.constFirst());
    return QString();
}

bool Calls::isPhoneNumber(const QString &text)
{
    static const QRegularExpression numberPattern(QStringLiteral("^[0-9+*#()/. -]*[0-9][0-9+*#()/. -]*$"));
    return numberPattern.match(text).hasMatch();
}

QList<Calls::Contact> Calls::findContacts(const QString &query, int limit) const
{
    // Full name first, so that "Ana" does not also match "Anamarija"
    const KPeople::PersonsModel &persons = m_bluetooth->persons();
    QList<QModelIndex> matches;
    for (const bool partial : {false, true}) {
        for (int row = 0; row < persons.rowCount(); ++row) {
            const QModelIndex person = persons.index(row);
            const QString name = person.data(KPeople::PersonsModel::FormattedNameRole).toString();
            if (partial ? name.contains(query, Qt::CaseInsensitive) : name.compare(query, Qt::CaseInsensitive) == 0) {
                matches.append(person);
            }
        }
        if (!matches.isEmpty()) {
            break;
        }
    }

    QList<Contact> contacts;
    for (const QModelIndex &person : matches.mid(0, limit)) {
        Contact contact;
        contact.name = person.data(KPeople::PersonsModel::FormattedNameRole).toString();
        const auto vcard = person.data(KPeople::PersonsModel::PersonVCardRole).value<KPeople::AbstractContact::Ptr>();
        const QStringList vcardNumbers = vcard ? vcard->customProperty(KPeople::AbstractContact::AllPhoneNumbersProperty).toStringList() : QStringList();
        for (const QString &number : vcardNumbers) {
            if (number.trimmed().isEmpty()) {
                continue;
            }
            const PhoneNumberMatcher matcher(number);
            if (std::none_of(contact.numbers.cbegin(), contact.numbers.cend(), [&matcher](const QString &known) {
                    return matcher.matches(known);
                })) {
                contact.numbers.append(number);
            }
        }
        contacts.append(contact);
    }
    return contacts;
}

void Calls::dialNumber(const QString &number)
{
    // Starting a second call would also move the current call's audio
    if (!gatewayConnected() || !m_calls.isEmpty()) {
        return;
    }
    // ATD takes the number as is, so drop the formatting
    static const QRegularExpression formatting(QStringLiteral("[^0-9+*#]"));
    QString digits = number;
    digits.remove(formatting);

    setRejectSco(false);
    QDBusMessage request = QDBusMessage::createMethodCall(TELEPHONY_SERVICE, m_gatewayPath, GATEWAY_INTERFACE, QStringLiteral("Dial"));
    request.setArguments({digits});
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, number](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (watcher->isError()) {
            qCWarning(PHONEKOMPANIOND) << "Dial failed" << watcher->error().message();
            if (m_calls.isEmpty()) {
                setRejectSco(true);
            }
            auto *notification = new KNotification(QStringLiteral("error"));
            notification->setTitle(m_phone->name());
            notification->setText(i18n("Couldn't call %1", number));
            notification->sendEvent();
        }
    });
}

void Calls::declineCall(const QString &path)
{
    if (m_calls.contains(path)) {
        m_calls[path].declined = true;
        callMethod(path, CALL_INTERFACE, QStringLiteral("Hangup"));
    }
}

void Calls::refreshNotifications()
{
    const QStringList paths = m_calls.keys();
    for (const QString &path : paths) {
        const QString state = m_calls.value(path).properties.value(QStringLiteral("State")).toString();
        if (state != QLatin1String("disconnected")) {
            updateNotification(path, state);
        }
    }
}

void Calls::answerCall(const QString &path)
{
    if (!m_calls.contains(path)) {
        return;
    }
    m_calls[path].answered = true;
    // Call1.Answer takes only an incoming call; the audio stays where the current call has it
    if (m_calls.value(path).properties.value(QStringLiteral("State")).toString() == QLatin1String("waiting")) {
        // HoldAndAnswer needs an active call; with only a held one, AT+CHLD=2 also accepts the waiting call
        const bool active = std::any_of(m_calls.cbegin(), m_calls.cend(), [](const Call &call) {
            return call.properties.value(QStringLiteral("State")).toString() == QLatin1String("active");
        });
        callMethod(m_gatewayPath, GATEWAY_INTERFACE, active ? QStringLiteral("HoldAndAnswer") : QStringLiteral("SwapCalls"));
        return;
    }
    // The phone then moves the call audio to this computer by itself
    setRejectSco(false);
    callMethod(path, CALL_INTERFACE, QStringLiteral("Answer"));
}

void Calls::moveAudioToPc()
{
    if (!gatewayConnected() || m_bluetooth->isLocked() || m_calls.isEmpty()) {
        return;
    }
    setRejectSco(false);
    callMethod(m_gatewayPath, TRANSPORT_INTERFACE, QStringLiteral("Activate"));
}

void Calls::setRejectSco(bool reject)
{
    if (!m_gatewayPath.isEmpty()) {
        callMethod(m_gatewayPath,
                   PROPERTIES_INTERFACE,
                   QStringLiteral("Set"),
                   {TRANSPORT_INTERFACE, QStringLiteral("RejectSCO"), QVariant::fromValue(QDBusVariant(reject))});
    }
}

void Calls::restoreRejectSco(bool block)
{
    if (!m_bluetooth->gateways().contains(m_gatewayPath)) {
        return;
    }
    // false is PipeWire's default (bluez5.telephony.default-reject-sco)
    if (!block) {
        setRejectSco(false);
        return;
    }
    QDBusMessage request = QDBusMessage::createMethodCall(TELEPHONY_SERVICE, m_gatewayPath, PROPERTIES_INTERFACE, QStringLiteral("Set"));
    request.setArguments({TRANSPORT_INTERFACE, QStringLiteral("RejectSCO"), QVariant::fromValue(QDBusVariant(false))});
    QDBusConnection::sessionBus().call(request, QDBus::Block, 500);
}

void Calls::callMethod(const QString &path, const QString &interface, const QString &method, const QVariantList &arguments)
{
    QDBusMessage request = QDBusMessage::createMethodCall(TELEPHONY_SERVICE, path, interface, method);
    request.setArguments(arguments);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [path, interface, method](QDBusPendingCallWatcher *watcher) {
        watcher->deleteLater();
        if (watcher->isError()) {
            qCWarning(PHONEKOMPANIOND) << interface << method << "failed for" << path << watcher->error().message();
        }
    });
}

void Calls::closeNotifications(const Call &call)
{
    if (call.notification) {
        call.notification->close();
    }
    if (call.ringtone) {
        call.ringtone->close();
    }
}

void Calls::watchChildren(const QString &path, bool watch)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (watch) {
        bus.connect(TELEPHONY_SERVICE, path, OBJECT_MANAGER_INTERFACE, QStringLiteral("InterfacesAdded"), this, SLOT(interfacesAdded(QDBusMessage)));
        bus.connect(TELEPHONY_SERVICE, path, OBJECT_MANAGER_INTERFACE, QStringLiteral("InterfacesRemoved"), this, SLOT(interfacesRemoved(QDBusMessage)));
    } else {
        bus.disconnect(TELEPHONY_SERVICE, path, OBJECT_MANAGER_INTERFACE, QStringLiteral("InterfacesAdded"), this, SLOT(interfacesAdded(QDBusMessage)));
        bus.disconnect(TELEPHONY_SERVICE, path, OBJECT_MANAGER_INTERFACE, QStringLiteral("InterfacesRemoved"), this, SLOT(interfacesRemoved(QDBusMessage)));
    }
}

void Calls::watchProperties(const QString &path, bool watch)
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (watch) {
        bus.connect(TELEPHONY_SERVICE, path, PROPERTIES_INTERFACE, QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChanged(QDBusMessage)));
    } else {
        bus.disconnect(TELEPHONY_SERVICE, path, PROPERTIES_INTERFACE, QStringLiteral("PropertiesChanged"), this, SLOT(propertiesChanged(QDBusMessage)));
    }
}

void Calls::interfacesAdded(const QDBusMessage &message)
{
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2) {
        return;
    }
    const QString path = qdbus_cast<QDBusObjectPath>(arguments.at(0)).path();
    const InterfaceMap interfaces = qdbus_cast<InterfaceMap>(arguments.at(1));
    if (message.path() == m_gatewayPath) {
        addCall(path, interfaces);
    }
}

void Calls::interfacesRemoved(const QDBusMessage &message)
{
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2) {
        return;
    }
    const QString path = qdbus_cast<QDBusObjectPath>(arguments.at(0)).path();
    const QStringList interfaces = arguments.at(1).toStringList();
    if (message.path() == m_gatewayPath) {
        removeCall(path, interfaces);
    }
}

void Calls::propertiesChanged(const QDBusMessage &message)
{
    const QList<QVariant> arguments = message.arguments();
    if (arguments.size() < 2) {
        return;
    }
    const QString path = message.path();
    const QString interface = arguments.at(0).toString();
    const QVariantMap changed = qdbus_cast<QVariantMap>(arguments.at(1));

    if (interface == CALL_INTERFACE && m_calls.contains(path)) {
        Call &call = m_calls[path];
        const QString previousState = call.properties.value(QStringLiteral("State")).toString();
        for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
            call.properties.insert(it.key(), it.value());
        }
        if (changed.contains(QStringLiteral("LineIdentification"))) {
            resolveCaller(call);
        }
        qCDebug(PHONEKOMPANIOND) << "Call" << path << "changed" << changed;
        updateNotification(path, previousState);
        notifyCallChanged();
    } else if (interface == TRANSPORT_INTERFACE && path == m_gatewayPath) {
        qCDebug(PHONEKOMPANIOND) << "Audio transport changed" << changed;
        if (changed.contains(QStringLiteral("State"))) {
            m_audioOnPc = changed.value(QStringLiteral("State")).toString() == QLatin1String("active");
            refreshNotifications();
            notifyCallChanged();
        }
    }
}

#include "moc_calls.cpp"
