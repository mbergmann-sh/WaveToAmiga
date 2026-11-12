// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "MainWindow.h"
#include "PathUtil.h"
#include "core/Converter.h"

#include <QApplication>
#include <QAudioOutput>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMediaPlayer>
#include <QPixmap>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#include <atomic>
#include <cmath>

// Verdeckter Startup-Test (--selftest <logdatei>): prüft, ob Qt-Plattform-Plugin, Multimedia-Backend,
// Ressourcen und die eingebundene SoX-Bibliothek funktionieren (inkl. je einer Test-Konvertierung in
// 8SVX, 8SVX-Fibonacci, AIFF, 16SV und MAUD) – ohne ein Fenster zu öffnen.
// Wird vom Build-Schritt "verify_deploy.ps1" mit minimalem PATH aufgerufen, um zu beweisen, dass der
// Deploy-Ordner/Installer vollständig ist (also nicht nur dank Qt/MinGW im PATH des Entwicklers läuft).
static bool writeTestWav(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    const quint32 frames = 4000, rate = 8000;
    QByteArray d;
    auto put16 = [&](quint16 v) { d.append(char(v & 0xFF)); d.append(char(v >> 8)); };
    auto put32 = [&](quint32 v) { put16(v & 0xFFFF); put16(v >> 16); };
    d.append("RIFF", 4); put32(36 + frames * 2); d.append("WAVEfmt ", 8);
    put32(16); put16(1); put16(1); put32(rate); put32(rate * 2); put16(2); put16(16);
    d.append("data", 4); put32(frames * 2);
    for (quint32 i = 0; i < frames; ++i) put16(quint16(qint16(12000 * std::sin(2 * 3.14159265358979 * 440.0 * i / rate))));
    return f.write(d) == d.size();
}

static int runSelfTest(const QString& logPath) {
    QStringList log;
    int rc = 0;
    log << QStringLiteral("Qt %1, Plattform-Plugin: %2").arg(QString::fromLatin1(qVersion()), QGuiApplication::platformName());
    log << QStringLiteral("libsox %1").arg(QString::fromStdString(w2a::soxVersion()));
    {
        QMediaPlayer player;
        QAudioOutput out;
        player.setAudioOutput(&out);
        const bool ok = player.isAvailable();
        log << QStringLiteral("Multimedia-Backend: %1").arg(ok ? QStringLiteral("verfuegbar") : QStringLiteral("FEHLT"));
        if (!ok) rc = 3;
    }
    if (QPixmap(QStringLiteral(":/icons/app_256.png")).isNull()) {
        log << QStringLiteral("Ressourcen: FEHLER");
        rc = 4;
    }
    QTemporaryDir tmp;
    if (!tmp.isValid()) {
        log << QStringLiteral("Temp-Ordner: FEHLER");
        rc = 5;
    } else {
        const QString wav = tmp.filePath(QStringLiteral("t.wav"));
        if (!writeTestWav(wav)) { log << QStringLiteral("Test-WAV: FEHLER"); rc = 5; }
        std::atomic<bool> noCancel{false};
        struct Case { const char* name; w2a::Target t; bool fib; };
        const Case cases[] = {{"8SVX", w2a::Target::Svx8, false}, {"8SVX-Fibonacci", w2a::Target::Svx8, true},
                              {"AIFF", w2a::Target::Aiff, false}, {"16SV", w2a::Target::Svx16, false},
                              {"MAUD", w2a::Target::Maud, false}};
        for (const Case& c : cases) {
            w2a::ConvertOptions o;
            o.target = c.t;
            o.fibonacci = c.fib;
            const QString outPath = tmp.filePath(QStringLiteral("o_%1.%2").arg(QString::fromLatin1(c.name),
                                                 QString::fromLatin1(w2a::targetInfo(c.t).extension)));
            const w2a::ConvertResult r = w2a::convertWav(toFsPath(wav), toFsPath(outPath), o, noCancel);
            const bool ok = r.ok && QFileInfo(outPath).size() > 44;
            log << QStringLiteral("Konverter %1: %2").arg(QString::fromLatin1(c.name), ok ? QStringLiteral("OK") : QStringLiteral("FEHLER ") + QString::fromStdString(r.error));
            if (!ok && rc == 0) rc = 6;
        }
    }
    log << (rc == 0 ? QStringLiteral("OK") : QStringLiteral("FEHLER"));

    if (!logPath.isEmpty()) {
        QFile f(logPath);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) QTextStream(&f) << log.join(QLatin1Char('\n')) << '\n';
    }
    return rc;
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("WaveToAmiga"));
    QCoreApplication::setApplicationName(QStringLiteral("WaveToAmiga"));
    QCoreApplication::setApplicationVersion(QStringLiteral(W2A_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("WaveToAmiga"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/app_256.png")));

    const QStringList args = app.arguments();
    const int st = args.indexOf(QStringLiteral("--selftest"));
    if (st >= 0) return runSelfTest(st + 1 < args.size() ? args.at(st + 1) : QString());

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("WAV → Amiga-Soundformate (8SVX, AIFF, 16SV, MAUD)"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("dateien"), QObject::tr("WAV-Dateien oder Ordner (optional)"));
    parser.process(app);

    w2a::initSox();
    MainWindow w;
    w.show();
    if (!parser.positionalArguments().isEmpty()) w.addPaths(parser.positionalArguments());
    return app.exec();
}
