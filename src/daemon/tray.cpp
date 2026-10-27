/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "tray.h"

#include <QAction>
#include <QMenu>
#include <QProcess>

#include <KLocalizedString>
#include <KStatusNotifierItem>

#include "calls.h"
#include "findexecutable.h"
#include "phone.h"
#include "phonekompaniond_debug.h"
#include "phonemanager.h"
#include "scrcpy.h"

#include <functional>

Tray::Tray(PhoneManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_item(new KStatusNotifierItem(this))
{
    m_item->setCategory(KStatusNotifierItem::Communications);
    m_item->setIconByName(QStringLiteral("smartphone"));
    m_item->setTitle(i18n("Phone Kompanion"));

    QAction *open = m_item->contextMenu()->addAction(QIcon::fromTheme(QStringLiteral("window")), i18n("Open Phone Kompanion"));
    connect(open, &QAction::triggered, this, [this] {
        openWindow();
    });
    // The phones' actions go before this, so they stay above Quit
    m_phonesEnd = m_item->contextMenu()->addSeparator();
    connect(m_item, &KStatusNotifierItem::activateRequested, this, [this] {
        openWindow();
    });

    connect(m_manager, &PhoneManager::phonesChanged, this, &Tray::update);
    update();
}

void Tray::update()
{
    QStringList lines;
    bool connected = false;
    const QList<Phone *> phones = m_manager->allPhones();
    for (const Phone *phone : phones) {
        if (phone->isReachable()) {
            lines << i18nc("@info:tooltip phone name", "%1: connected", phone->name());
        } else if (phone->callsConnected()) {
            lines << i18nc("@info:tooltip phone name", "%1: connected over Bluetooth", phone->name());
        } else {
            lines << i18nc("@info:tooltip phone name", "%1: not connected", phone->name());
        }
        connected |= phone->isReachable() || phone->callsConnected();
    }
    if (!m_manager->isKdeConnectRunning()) {
        lines = {i18n("KDE Connect is not running")};
    } else if (lines.isEmpty()) {
        lines << i18n("No phones paired in KDE Connect");
    }

    m_item->setToolTip(QStringLiteral("smartphone"), i18n("Phone Kompanion"), lines.join(QLatin1Char('\n')));
    // Passive moves the icon into the tray's hidden items
    m_item->setStatus(connected ? KStatusNotifierItem::Active : KStatusNotifierItem::Passive);
    updateMenu();
}

void Tray::updateMenu()
{
    // Deleted later, as this may run while one of the actions is triggered
    QMenu *menu = m_item->contextMenu();
    for (QAction *action : std::as_const(m_phoneActions)) {
        menu->removeAction(action);
        action->deleteLater();
    }
    m_phoneActions.clear();

    const QList<Phone *> phones = m_manager->allPhones();
    for (Phone *phone : phones) {
        // Calls also work over Bluetooth alone, mirroring needs KDE Connect's network connection
        if (!phone->isReachable() && !phone->callsConnected()) {
            continue;
        }
        Scrcpy *scrcpy = phone->scrcpy();
        Calls *calls = phone->calls();

        const auto addAction = [this, menu, phone](const QString &icon, const QString &text, const std::function<QString()> &run) {
            auto action = new QAction(QIcon::fromTheme(icon), text, menu);
            menu->insertAction(m_phonesEnd, action);
            connect(action, &QAction::triggered, phone, [phone, run] {
                phone->notifyError(run());
            });
            m_phoneActions.append(action);
            return action;
        };
        m_phoneActions.append(menu->insertSection(m_phonesEnd, phone->name()));

        addAction(QStringLiteral("call-outgoing"), i18n("Phone Calls…"), [this, phone] {
            openWindow({QStringLiteral("--phone"), phone->id(), QStringLiteral("--dialer")});
            return QString();
        });
        QAction *answer = addAction(QStringLiteral("call-start"), QString(), [calls] {
            calls->answer();
            return QString();
        });
        QAction *hangUp = addAction(QStringLiteral("call-stop"), QString(), [calls] {
            calls->hangUp();
            return QString();
        });
        QAction *mute = addAction(QString(), QString(), [calls] {
            calls->setMicrophoneMuted(!calls->microphoneMuted());
            return QString();
        });
        const auto updateCall = [calls, answer, hangUp, mute] {
            const QString state = calls->callState();
            const bool ringing = state == QLatin1String("incoming") || state == QLatin1String("waiting");
            // "&" would mark a shortcut in the menu
            QString caller = calls->callName().isEmpty() ? calls->callNumber() : calls->callName();
            caller = caller.isEmpty() ? i18n("unknown number") : caller.replace(QLatin1Char('&'), QStringLiteral("&&"));
            answer->setVisible(ringing);
            answer->setText(i18nc("@action:inmenu %1 is a contact name or phone number", "Answer Call from %1", caller));
            hangUp->setVisible(!state.isEmpty());
            hangUp->setText(ringing ? i18n("Decline Call") : i18nc("@action:inmenu %1 is a contact name or phone number", "Hang Up Call with %1", caller));
            mute->setVisible(state == QLatin1String("active") && calls->audioOnPc() && calls->microphoneMuteSupported());
            mute->setIcon(
                QIcon::fromTheme(calls->microphoneMuted() ? QStringLiteral("microphone-sensitivity-muted") : QStringLiteral("microphone-sensitivity-high")));
            mute->setText(calls->microphoneMuted() ? i18n("Unmute Microphone") : i18n("Mute Microphone"));
        };
        updateCall();
        // Only these entries change with the call; the menu is rebuilt when the phones change
        connect(calls, &Calls::callChanged, answer, updateCall);
        // For microphoneMuteSupported
        connect(calls, &Calls::statusChanged, answer, updateCall);

        if (!phone->isReachable()) {
            continue;
        }
        if (phone->messagesAvailable()) {
            addAction(QStringLiteral("dialog-messages"), i18n("Messages…"), [this, phone] {
                openWindow({QStringLiteral("--phone"), phone->id(), QStringLiteral("--messages")});
                return QString();
            });
        }
        addAction(QStringLiteral("video-display"), i18n("Mirror Screen"), [scrcpy] {
            return scrcpy->mirrorScreen();
        });
        addAction(QStringLiteral("view-app-grid"), i18n("Open App…"), [this, phone] {
            openWindow({QStringLiteral("--phone"), phone->id(), QStringLiteral("--apps")});
            return QString();
        });
        addAction(QStringLiteral("camera-photo"), i18n("Show Camera"), [scrcpy] {
            return scrcpy->showCamera(QString());
        });
        QAction *webcam = addAction(QStringLiteral("camera-web"), QString(), [scrcpy] {
            if (scrcpy->webcamActive()) {
                scrcpy->stopWebcam();
                return QString();
            }
            return scrcpy->startWebcam(QString());
        });
        const auto updateWebcam = [webcam, scrcpy] {
            webcam->setText(scrcpy->webcamActive() ? i18n("Stop Webcam") : i18n("Use Camera as Webcam"));
        };
        updateWebcam();
        connect(scrcpy, &Scrcpy::statusChanged, webcam, updateWebcam);
    }
}

void Tray::openWindow(const QStringList &arguments)
{
    const QString app = findOwnExecutable(QStringLiteral("phonekompanion"));

    QProcess process;
    process.setProgram(app);
    process.setArguments(arguments);
    // Lets the window take focus on Wayland; there is no token when opened from the menu
    if (const QString token = m_item->providedToken(); !token.isEmpty()) {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("XDG_ACTIVATION_TOKEN"), token);
        process.setProcessEnvironment(environment);
    }
    if (app.isEmpty() || !process.startDetached()) {
        qCWarning(PHONEKOMPANIOND) << "Could not start phonekompanion" << app;
    }
}

#include "moc_tray.cpp"
