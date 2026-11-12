// SPDX-License-Identifier: MIT
#pragma once

#include "core/Converter.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QThreadPool>

#include <atomic>

// Führt mehrere WAV->Amiga-Aufträge parallel in einem Thread-Pool aus und meldet
// Fortschritt/Ergebnis per Signal an den GUI-Thread.
class ConvertManager : public QObject {
    Q_OBJECT
public:
    struct Job {
        int id = 0;
        QString inPath;
        QString outPath;
        w2a::ConvertOptions options;
    };
    enum Result { Success = 0, Failure = 1, Cancelled = 2 };

    explicit ConvertManager(QObject* parent = nullptr);
    ~ConvertManager() override;

    void setMaxParallel(int n);
    void start(const QList<Job>& jobs);   // ignoriert, solange noch Aufträge laufen
    void cancelAll();
    bool isRunning() const { return pending_ > 0; }

signals:
    void jobStarted(int id);
    void jobProgress(int id, int percent);
    void jobFinished(int id, int result, const QString& message);  // message: Zielpfad bzw. Fehlertext
    void allFinished();

private:
    class Task;
    void handleFinished(int id, int result, const QString& message);

    QThreadPool pool_;
    std::atomic<bool> cancel_{false};
    int pending_ = 0;
};
