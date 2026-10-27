/*
 * SPDX-FileCopyrightText: 2026 Rok Krajnc <rok.krajnc@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QCoreApplication>
#include <QStandardPaths>

/**
 * One of Phone Kompanion's programs: next to this program first, so a build that is not
 * installed uses its own programs, then in PATH. Empty if not found.
 */
inline QString findOwnExecutable(const QString &name)
{
    const QString path = QStandardPaths::findExecutable(name, {QCoreApplication::applicationDirPath()});
    return path.isEmpty() ? QStandardPaths::findExecutable(name) : path;
}
