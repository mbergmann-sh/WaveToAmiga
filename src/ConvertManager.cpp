// SPDX-License-Identifier: MIT
#include "ConvertManager.h"

#include "PathUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QRunnable>

class ConvertManager::Task : public QRunnable {
public:
    Task(ConvertManager* mgr, const Job& job) : mgr_(mgr), job_(job) { setAutoDelete(true); }

    void run() override {
        ConvertManager* m = mgr_;
        const int id = job_.id;

        if (m->cancel_.load()) {
            finish(Cancelled, QObject::tr("Abgebrochen"));
            return;
        }
        QMetaObject::invokeMethod(m, [m, id] { emit m->jobStarted(id); }, Qt::QueuedConnection);

        QDir().mkpath(QFileInfo(job_.outPath).absolutePath());
        // Es wird in eine ".part"-Datei geschrieben und erst nach Erfolg umbenannt,
        // damit nie halbfertige Dateien unter dem Zielnamen liegen.
        const QString part = job_.outPath + QStringLiteral(".part");

        auto progress = [m, id](int pct) {
            QMetaObject::invokeMethod(m, [m, id, pct] { emit m->jobProgress(id, pct); }, Qt::QueuedConnection);
        };
        const w2a::ConvertResult r = w2a::convertWav(toFsPath(job_.inPath), toFsPath(part),
                                                     job_.options, m->cancel_, progress);
        if (r.ok) {
            QFile::remove(job_.outPath);  // relevant bei Richtlinie „Überschreiben“
            if (QFile::rename(part, job_.outPath)) {
                finish(Success, job_.outPath);
            } else {
                QFile::remove(part);
                finish(Failure, QObject::tr("Zieldatei kann nicht geschrieben werden: %1").arg(job_.outPath));
            }
        } else {
            QFile::remove(part);
            finish(r.cancelled ? Cancelled : Failure, QString::fromStdString(r.error));
        }
    }

private:
    void finish(int result, const QString& message) {
        ConvertManager* m = mgr_;
        const int id = job_.id;
        QMetaObject::invokeMethod(m, [m, id, result, message] { m->handleFinished(id, result, message); },
                                  Qt::QueuedConnection);
    }

    ConvertManager* mgr_;
    Job job_;
};

ConvertManager::ConvertManager(QObject* parent) : QObject(parent) {}

ConvertManager::~ConvertManager() {
    cancel_ = true;
    pool_.clear();
    pool_.waitForDone();
}

void ConvertManager::setMaxParallel(int n) { pool_.setMaxThreadCount(qMax(1, n)); }

void ConvertManager::start(const QList<Job>& jobs) {
    if (pending_ > 0 || jobs.isEmpty()) return;
    cancel_ = false;
    pending_ = jobs.size();
    for (const Job& j : jobs) pool_.start(new Task(this, j));
}

void ConvertManager::cancelAll() { cancel_ = true; }

void ConvertManager::handleFinished(int id, int result, const QString& message) {
    if (pending_ > 0) --pending_;
    emit jobFinished(id, result, message);
    if (pending_ == 0) emit allFinished();
}
