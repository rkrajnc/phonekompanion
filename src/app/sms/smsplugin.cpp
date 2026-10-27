/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include <QQmlEngine>
#include <QQmlEngineExtensionPlugin>

#include "thumbnailsprovider.h"

extern void qml_register_types_io_github_rkrajnc_phonekompanion_sms();
Q_GHS_KEEP_REFERENCE(qml_register_types_io_github_rkrajnc_phonekompanion_sms)

/**
 * The SMS QML module, with the image provider for the photos in the conversations
 */
class PhoneKompanionSmsPlugin : public QQmlEngineExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlEngineExtensionInterface_iid)

public:
    explicit PhoneKompanionSmsPlugin(QObject *parent = nullptr)
        : QQmlEngineExtensionPlugin(parent)
    {
        // Keeps the type registration of the static module from being dropped by the linker
        volatile auto registration = &qml_register_types_io_github_rkrajnc_phonekompanion_sms;
        Q_UNUSED(registration)
    }

    void initializeEngine(QQmlEngine *engine, const char *uri) override
    {
        QQmlEngineExtensionPlugin::initializeEngine(engine, uri);
        engine->addImageProvider(QStringLiteral("thumbnailsProvider"), new ThumbnailsProvider);
    }
};

#include "smsplugin.moc"
