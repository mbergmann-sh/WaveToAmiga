// SPDX-License-Identifier: MIT
#pragma once

#include <QFile>
#include <QString>

#include <filesystem>

// QString -> std::filesystem::path, korrekt für Unicode-Pfade unter Windows und
// für die lokale Dateisystem-Kodierung unter Linux.
inline std::filesystem::path toFsPath(const QString& s) {
#ifdef Q_OS_WIN
    return std::filesystem::path(s.toStdWString());
#else
    return std::filesystem::path(QFile::encodeName(s).toStdString());
#endif
}
