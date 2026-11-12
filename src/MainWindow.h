// SPDX-License-Identifier: MIT
#pragma once

#include "ConvertManager.h"

#include <QHash>
#include <QMainWindow>
#include <QMediaPlayer>
#include <QSet>
#include <QUrl>

class QAction;
class QAudioOutput;
class QComboBox;
class QTimer;
class QLabel;
class QLayout;
class QProgressBar;
class QSlider;
class QTableWidgetItem;
class FileTable;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Fügt Dateien und/oder Ordner (rekursiv, nur *.wav) zur Liste hinzu.
    void addPaths(const QStringList& paths);

protected:
    void closeEvent(QCloseEvent* e) override;
    void moveEvent(QMoveEvent* e) override;
    void resizeEvent(QResizeEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private slots:
    void onAddFiles();
    void onRemoveSelected();
    void onClearList();
    void onStartConversion();
    void onCancelConversion();
    void onSettings();
    void onTargetChanged();
    void saveWindowLayout();
    void onAbout();
    void onUrlsDropped(const QList<QUrl>& urls);
    void onTableContextMenu(const QPoint& pos);

    void onJobStarted(int id);
    void onJobProgress(int id, int percent);
    void onJobFinished(int id, int result, const QString& message);
    void onAllFinished();

    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    void onPositionChanged(qint64 pos);
    void onDurationChanged(qint64 dur);
    void onPlaybackStateChanged(QMediaPlayer::PlaybackState state);

private:
    enum Column { ColName = 0, ColInfo, ColProgress, ColStatus, ColCount };
    enum Role { RolePath = Qt::UserRole, RoleId, RoleState, RoleOutput, RoleSig };
    enum class RowState { Pending, Running, Done, Failed, Cancelled, Skipped };
    enum class AddResult { Added, Duplicate, Invalid };

    void buildUi();
    void buildActions();
    void buildPlayerBar(QWidget* parent, QLayout* layout);

    AddResult addFile(const QString& path, QString* error);
    void collectWavFiles(const QStringList& paths, QStringList& out) const;
    int rowForId(int id) const;
    QString rowPath(int row) const;
    RowState rowState(int row) const;
    void setRowState(int row, RowState st, const QString& text);
    void setRowProgress(int row, int percent);
    void removeRows(QList<int> rows);

    QString resolveOutputPath(const QString& inPath, QSet<QString>& reserved, bool* skip) const;
    void updateOverallProgress();
    void updateUiState();
    void refreshStatusInfo();

    void playPath(const QString& path);
    void playRow(int row, bool convertedFile);
    int currentRow() const;
    void updatePlaybackUi();
    static QString formatTime(qint64 ms);

    FileTable* table_ = nullptr;
    QComboBox* targetCombo_ = nullptr;   // Zielformat-Chooser in der Werkzeugleiste
    QTimer* layoutTimer_ = nullptr;      // verzögertes Speichern von Fenstergröße/-position
    ConvertManager* manager_ = nullptr;

    // Player
    QMediaPlayer* player_ = nullptr;
    QAudioOutput* audioOut_ = nullptr;
    QString playingPath_;
    QSlider* positionSlider_ = nullptr;
    QSlider* volumeSlider_ = nullptr;
    QLabel* timeLabel_ = nullptr;
    QLabel* nowPlayingLabel_ = nullptr;

    // Aktionen
    QAction *addAct_ = nullptr, *removeAct_ = nullptr, *clearAct_ = nullptr;
    QAction *convertAct_ = nullptr, *cancelAct_ = nullptr, *settingsAct_ = nullptr;
    QAction *playAct_ = nullptr, *pauseAct_ = nullptr, *stopAct_ = nullptr;

    // Statuszeile
    QProgressBar* overallBar_ = nullptr;
    QLabel* summaryLabel_ = nullptr;
    QLabel* settingsLabel_ = nullptr;

    QSet<QString> knownPaths_;
    QHash<int, int> jobPercent_;
    int batchSize_ = 0;
    int batchOk_ = 0, batchFailed_ = 0, batchCancelled_ = 0, batchSkipped_ = 0;
    int nextId_ = 1;
};
