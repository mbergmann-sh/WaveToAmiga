// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "AppSettings.h"

#include <QDir>
#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>

using w2a::MaudCoding;
using w2a::Target;

// ================================================================================ Anzeigetexte
QString targetLabel(Target t) {
    switch (t) {
    case Target::Svx8:  return QObject::tr("IFF-8SVX (8 Bit)");
    case Target::Aiff:  return QObject::tr("AIFF (8–32 Bit)");
    case Target::Svx16: return QObject::tr("IFF-16SV (16 Bit)");
    case Target::Maud:  return QObject::tr("IFF-MAUD");
    }
    return QString();
}

QString targetToolTip(Target t) {
    switch (t) {
    case Target::Svx8:
        return QObject::tr("Klassisches Amiga-Sample: 8 Bit signiert, unkomprimiert (optional Fibonacci-Delta-Kompression, "
                           "siehe Voreinstellungen). Mono/Stereo/Quad.");
    case Target::Aiff:
        return QObject::tr("Audio Interchange File Format (IFF-Container): 8, 16, 24 oder 32 Bit, mehrere Kanäle.");
    case Target::Svx16:
        return QObject::tr("IFF-16SV: wie 8SVX, aber mit 16-Bit-Samples (Big-Endian). Mono/Stereo/Quad.");
    case Target::Maud:
        return QObject::tr("IFF-MAUD (MacroSystem): 16 Bit, 8 Bit, µ-law oder A-law. Mono/Stereo.");
    }
    return QString();
}

QString maudCodingKey(MaudCoding c) {
    switch (c) {
    case MaudCoding::Pcm16: return QStringLiteral("pcm16");
    case MaudCoding::Pcm8:  return QStringLiteral("pcm8");
    case MaudCoding::ULaw:  return QStringLiteral("ulaw");
    case MaudCoding::ALaw:  return QStringLiteral("alaw");
    }
    return QStringLiteral("pcm16");
}

MaudCoding maudCodingFromKey(const QString& key) {
    for (MaudCoding c : {MaudCoding::Pcm16, MaudCoding::Pcm8, MaudCoding::ULaw, MaudCoding::ALaw})
        if (maudCodingKey(c) == key) return c;
    return MaudCoding::Pcm16;
}

QString maudCodingLabel(MaudCoding c) {
    switch (c) {
    case MaudCoding::Pcm16: return QObject::tr("16 Bit PCM (Standard)");
    case MaudCoding::Pcm8:  return QObject::tr("8 Bit PCM (unsigniert)");
    case MaudCoding::ULaw:  return QObject::tr("8 Bit µ-law");
    case MaudCoding::ALaw:  return QObject::tr("8 Bit A-law");
    }
    return QString();
}

// ================================================================================ Einstellungen
namespace AppSettings {

namespace {
QString musicDir() {
    QString d = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (d.isEmpty()) d = QDir::homePath();
    return d;
}
} // namespace

QString defaultSourceDir() { return musicDir(); }
QString defaultTargetDir() { return QDir(musicDir()).filePath(QStringLiteral("WaveToAmiga")); }

QString sourceDir() {
    const QString d = QSettings().value(QStringLiteral("paths/source")).toString();
    return d.isEmpty() ? defaultSourceDir() : d;
}
void setSourceDir(const QString& dir) { QSettings().setValue(QStringLiteral("paths/source"), dir); }

QString targetDir() {
    const QString d = QSettings().value(QStringLiteral("paths/target")).toString();
    return d.isEmpty() ? defaultTargetDir() : d;
}
void setTargetDir(const QString& dir) { QSettings().setValue(QStringLiteral("paths/target"), dir); }

bool targetSameAsSource() { return QSettings().value(QStringLiteral("paths/targetSameAsSource"), false).toBool(); }
void setTargetSameAsSource(bool on) { QSettings().setValue(QStringLiteral("paths/targetSameAsSource"), on); }

Target target() {
    const QString key = QSettings().value(QStringLiteral("converter/target"), QStringLiteral("8svx")).toString();
    return w2a::targetFromKey(key.toStdString(), Target::Svx8);
}
void setTarget(Target t) { QSettings().setValue(QStringLiteral("converter/target"), QString::fromLatin1(w2a::targetInfo(t).key)); }

bool fibonacci() { return QSettings().value(QStringLiteral("converter/fibonacci"), false).toBool(); }
void setFibonacci(bool on) { QSettings().setValue(QStringLiteral("converter/fibonacci"), on); }

int aiffBits() {
    const int b = QSettings().value(QStringLiteral("converter/aiffBits"), 0).toInt();
    return (b == 8 || b == 16 || b == 24 || b == 32) ? b : 0;
}
void setAiffBits(int bits) { QSettings().setValue(QStringLiteral("converter/aiffBits"), bits); }

MaudCoding maudCoding() {
    return maudCodingFromKey(QSettings().value(QStringLiteral("converter/maud"), QStringLiteral("pcm16")).toString());
}
void setMaudCoding(MaudCoding c) { QSettings().setValue(QStringLiteral("converter/maud"), maudCodingKey(c)); }

int targetRate() { return std::clamp(QSettings().value(QStringLiteral("converter/rate"), 0).toInt(), 0, 384000); }
void setTargetRate(int hz) { QSettings().setValue(QStringLiteral("converter/rate"), std::clamp(hz, 0, 384000)); }

bool forceMono() { return QSettings().value(QStringLiteral("converter/mono"), false).toBool(); }
void setForceMono(bool on) { QSettings().setValue(QStringLiteral("converter/mono"), on); }

w2a::ConvertOptions convertOptions() {
    w2a::ConvertOptions o;
    o.target = target();
    o.fibonacci = fibonacci();
    o.aiffBits = aiffBits();
    o.maud = maudCoding();
    o.targetRate = targetRate();
    o.forceMono = forceMono();
    return o;
}

int defaultParallelJobs() { return std::clamp(QThread::idealThreadCount() / 2, 1, 4); }
int parallelJobs() {
    const int maxJobs = std::max(1, QThread::idealThreadCount());
    return std::clamp(QSettings().value(QStringLiteral("processing/parallel"), defaultParallelJobs()).toInt(), 1, maxJobs);
}
void setParallelJobs(int n) { QSettings().setValue(QStringLiteral("processing/parallel"), std::max(1, n)); }

OverwritePolicy overwritePolicy() {
    const int v = QSettings().value(QStringLiteral("processing/overwrite"), int(OverwritePolicy::Rename)).toInt();
    return (v >= 0 && v <= 2) ? static_cast<OverwritePolicy>(v) : OverwritePolicy::Rename;
}
void setOverwritePolicy(OverwritePolicy p) { QSettings().setValue(QStringLiteral("processing/overwrite"), int(p)); }

int volume() { return std::clamp(QSettings().value(QStringLiteral("player/volume"), 80).toInt(), 0, 100); }
void setVolume(int v) { QSettings().setValue(QStringLiteral("player/volume"), std::clamp(v, 0, 100)); }

QByteArray windowGeometry() { return QSettings().value(QStringLiteral("window/geometry")).toByteArray(); }
void setWindowGeometry(const QByteArray& g) { QSettings().setValue(QStringLiteral("window/geometry"), g); }
QByteArray windowState() { return QSettings().value(QStringLiteral("window/state")).toByteArray(); }
void setWindowState(const QByteArray& s) { QSettings().setValue(QStringLiteral("window/state"), s); }

void resetToDefaults() {
    QSettings s;
    s.remove(QStringLiteral("paths"));
    s.remove(QStringLiteral("processing"));
    s.remove(QStringLiteral("converter/fibonacci"));
    s.remove(QStringLiteral("converter/aiffBits"));
    s.remove(QStringLiteral("converter/maud"));
    s.remove(QStringLiteral("converter/rate"));
    s.remove(QStringLiteral("converter/mono"));
}

} // namespace AppSettings
