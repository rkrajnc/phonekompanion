/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QVariant>

/**
 * Converts a setting's new value to the type of its default. Returns false for a value that
 * does not fit, e.g. "no" for a bool, which QVariant would take as true.
 */
inline bool convertSetting(QVariant &value, const QVariant &defaultValue)
{
    switch (defaultValue.typeId()) {
    case QMetaType::Bool:
    case QMetaType::QString:
        return value.typeId() == defaultValue.typeId();
    case QMetaType::Int:
        // Numbers come as int over D-Bus, and as double from QML
        switch (value.typeId()) {
        case QMetaType::Int:
        case QMetaType::UInt:
        case QMetaType::LongLong:
        case QMetaType::ULongLong:
        case QMetaType::Double:
            return value.convert(defaultValue.metaType());
        }
        return false;
    }
    return false;
}
