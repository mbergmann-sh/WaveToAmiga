// SPDX-License-Identifier: MIT
#include "MainWindow.h"

#include "AboutDialog.h"
#include "AppSettings.h"
#include "FileTable.h"
#include "PathUtil.h"
#include "SettingsDialog.h"
#include "core/WavReader.h"

#include <QAction>
#include <QApplication>
#include <QAudioOutput>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QSlider>
#include <QStatusBar>
#include <QToolBar>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace {

QIcon icon(const char* name) { return QIcon(QStringLiteral(":/icons/%1.png").arg(QLatin1String(name))); }

// Kennung der Konvertierungs-Einstellungen: ändert sich Zielformat oder Option, werden bereits konvertierte
// Dateien beim nächsten Lauf erneut konvertiert.
QString optionsSignature(const w2a::ConvertOptions& o) {
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(QString::fromLatin1(w2a::targetInfo(o.target).key))
        .arg(int(o.fibonacci))
        .arg(o.aiffBits)
        .arg(maudCodingKey(o.maud))
        .arg(o.targetRate)
        .arg(int(o.forceMono));
}

QString pathKey(const QString& p) {
#ifdef Q_OS_WIN
    return QDir::cleanPath(p).toLower();
#else
    return QDir::cleanPath(p);
#endif
}

} // namespace

// ============================================================================ Aufbau
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("WaveToAmiga"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app_256.png")));
    setAcceptDrops(true);
    resize(980, 620);

    manager_ = new ConvertManager(this);
    connect(manager_, &ConvertManager::jobStarted, this, &MainWindow::onJobStarted);
    connect(manager_, &ConvertManager::jobProgress, this, &MainWindow::onJobProgress);
    connect(manager_, &ConvertManager::jobFinished, this, &MainWindow::onJobFinished);
    connect(manager_, &ConvertManager::allFinished, this, &MainWindow::onAllFinished);

    player_ = new QMediaPlayer(this);
    audioOut_ = new QAudioOutput(this);
    player_->setAudioOutput(audioOut_);
    audioOut_->setVolume(AppSettings::volume() / 100.0f);
    connect(player_, &QMediaPlayer::positionChanged, this, &MainWindow::onPositionChanged);
    connect(player_, &QMediaPlayer::durationChanged, this, &MainWindow::onDurationChanged);
    connect(player_, &QMediaPlayer::playbackStateChanged, this, &MainWindow::onPlaybackStateChanged);
    connect(player_, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString& msg) {
        statusBar()->showMessage(tr("Wiedergabefehler: %1").arg(msg), 8000);
    });

    buildActions();
    buildUi();

    // Fenstergröße, -position (und Werkzeugleisten-Zustand) vom letzten Start wiederherstellen
    layoutTimer_ = new QTimer(this);
    layoutTimer_->setSingleShot(true);
    layoutTimer_->setInterval(500);
    connect(layoutTimer_, &QTimer::timeout, this, &MainWindow::saveWindowLayout);
    const QByteArray geo = AppSettings::windowGeometry();
    if (!geo.isEmpty()) restoreGeometry(geo);
    const QByteArray st = AppSettings::windowState();
    if (!st.isEmpty()) restoreState(st);

    refreshStatusInfo();
    updateUiState();
    updatePlaybackUi();
}

MainWindow::~MainWindow() {
    player_->stop();
}

void MainWindow::buildActions() {
    addAct_ = new QAction(icon("add"), tr("Dateien hinzufügen …"), this);
    addAct_->setShortcut(QKeySequence::Open);
    addAct_->setToolTip(tr("WAV-Dateien auswählen (Mehrfachauswahl möglich)"));
    connect(addAct_, &QAction::triggered, this, &MainWindow::onAddFiles);

    removeAct_ = new QAction(icon("remove"), tr("Auswahl entfernen"), this);
    removeAct_->setShortcut(QKeySequence::Delete);
    connect(removeAct_, &QAction::triggered, this, &MainWindow::onRemoveSelected);

    clearAct_ = new QAction(icon("clear"), tr("Liste leeren"), this);
    connect(clearAct_, &QAction::triggered, this, &MainWindow::onClearList);

    convertAct_ = new QAction(icon("convert"), tr("Konvertieren"), this);
    convertAct_->setShortcut(Qt::CTRL | Qt::Key_Return);
    convertAct_->setToolTip(tr("Alle Dateien der Liste in das gewählte Amiga-Zielformat umwandeln"));
    connect(convertAct_, &QAction::triggered, this, &MainWindow::onStartConversion);

    cancelAct_ = new QAction(icon("cancel"), tr("Abbrechen"), this);
    cancelAct_->setShortcut(Qt::Key_Escape);
    connect(cancelAct_, &QAction::triggered, this, &MainWindow::onCancelConversion);

    settingsAct_ = new QAction(icon("settings"), tr("Voreinstellungen …"), this);
    settingsAct_->setShortcut(QKeySequence::Preferences);
    connect(settingsAct_, &QAction::triggered, this, &MainWindow::onSettings);

    playAct_ = new QAction(icon("play"), tr("Abspielen"), this);
    playAct_->setToolTip(tr("Ausgewählte Datei abspielen bzw. fortsetzen"));
    connect(playAct_, &QAction::triggered, this, &MainWindow::onPlayClicked);

    pauseAct_ = new QAction(icon("pause"), tr("Pause"), this);
    connect(pauseAct_, &QAction::triggered, this, &MainWindow::onPauseClicked);

    stopAct_ = new QAction(icon("stop"), tr("Stopp"), this);
    connect(stopAct_, &QAction::triggered, this, &MainWindow::onStopClicked);
}

void MainWindow::buildUi() {
    // ---- Menü ---------------------------------------------------------------
    QMenu* fileMenu = menuBar()->addMenu(tr("&Datei"));
    fileMenu->addAction(addAct_);
    fileMenu->addAction(removeAct_);
    fileMenu->addAction(clearAct_);
    fileMenu->addSeparator();
    QAction* quitAct = fileMenu->addAction(tr("&Beenden"));
    quitAct->setShortcut(QKeySequence::Quit);
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    QMenu* convMenu = menuBar()->addMenu(tr("&Konvertierung"));
    convMenu->addAction(convertAct_);
    convMenu->addAction(cancelAct_);

    QMenu* playMenu = menuBar()->addMenu(tr("&Wiedergabe"));
    playMenu->addAction(playAct_);
    playMenu->addAction(pauseAct_);
    playMenu->addAction(stopAct_);

    QMenu* editMenu = menuBar()->addMenu(tr("&Extras"));
    editMenu->addAction(settingsAct_);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Hilfe"));
    QAction* aboutAct = helpMenu->addAction(icon("info"), tr("Über WaveToAmiga …"));
    connect(aboutAct, &QAction::triggered, this, &MainWindow::onAbout);
    QAction* aboutQtAct = helpMenu->addAction(tr("Über &Qt …"));
    connect(aboutQtAct, &QAction::triggered, qApp, &QApplication::aboutQt);

    // ---- Werkzeugleiste -----------------------------------------------------
    QToolBar* tb = addToolBar(tr("Werkzeuge"));
    tb->setObjectName(QStringLiteral("mainToolbar"));
    tb->setMovable(false);
    tb->setIconSize(QSize(28, 28));
    tb->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    tb->addAction(addAct_);
    tb->addAction(removeAct_);
    tb->addAction(clearAct_);
    tb->addSeparator();

    // ---- Zielformat-Chooser ("Konverter") ----
    auto* fmtLabel = new QLabel(tr(" Zielformat: "), tb);
    tb->addWidget(fmtLabel);
    targetCombo_ = new QComboBox(tb);
    targetCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    targetCombo_->setMinimumContentsLength(18);
    for (const auto& ti : w2a::allTargets()) {
        targetCombo_->addItem(targetLabel(ti.id), QString::fromLatin1(ti.key));
        targetCombo_->setItemData(targetCombo_->count() - 1, targetToolTip(ti.id), Qt::ToolTipRole);
    }
    targetCombo_->setCurrentIndex(qMax(0, targetCombo_->findData(QString::fromLatin1(w2a::targetInfo(AppSettings::target()).key))));
    targetCombo_->setToolTip(targetToolTip(AppSettings::target()));
    tb->addWidget(targetCombo_);
    connect(targetCombo_, &QComboBox::currentIndexChanged, this, [this](int) { onTargetChanged(); });
    tb->addSeparator();

    tb->addAction(convertAct_);
    tb->addAction(cancelAct_);
    QWidget* spacer = new QWidget(tb);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
    tb->addAction(settingsAct_);

    // ---- Zentrale Ansicht ---------------------------------------------------
    auto* central = new QWidget(this);
    auto* vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(8, 8, 8, 4);

    table_ = new FileTable(central);
    table_->setHintText(tr("WAV-Dateien oder Ordner hierher ziehen\noder über „Dateien hinzufügen …“ auswählen"));
    table_->setColumnCount(ColCount);
    table_->setHorizontalHeaderLabels({tr("Datei"), tr("Format"), tr("Fortschritt"), tr("Status")});
    table_->verticalHeader()->hide();
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->setWordWrap(false);
    table_->setTextElideMode(Qt::ElideMiddle);
    table_->setContextMenuPolicy(Qt::CustomContextMenu);
    QHeaderView* hh = table_->horizontalHeader();
    hh->setSectionResizeMode(ColName, QHeaderView::Stretch);
    hh->setSectionResizeMode(ColInfo, QHeaderView::ResizeToContents);
    hh->setSectionResizeMode(ColProgress, QHeaderView::Fixed);
    hh->setSectionResizeMode(ColStatus, QHeaderView::Stretch);
    table_->setColumnWidth(ColProgress, 140);
    vbox->addWidget(table_, 1);

    connect(table_, &FileTable::urlsDropped, this, &MainWindow::onUrlsDropped);
    connect(table_, &QWidget::customContextMenuRequested, this, &MainWindow::onTableContextMenu);
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this](int row, int) { playRow(row, false); });
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this] { updateUiState(); });

    buildPlayerBar(central, vbox);
    setCentralWidget(central);

    // ---- Statuszeile --------------------------------------------------------
    overallBar_ = new QProgressBar(this);
    overallBar_->setRange(0, 100);
    overallBar_->setFixedWidth(200);
    overallBar_->setVisible(false);
    summaryLabel_ = new QLabel(this);
    settingsLabel_ = new QLabel(this);
    statusBar()->addWidget(summaryLabel_, 1);
    statusBar()->addPermanentWidget(overallBar_);
    statusBar()->addPermanentWidget(settingsLabel_);
}

void MainWindow::buildPlayerBar(QWidget* parent, QLayout* layout) {
    auto* bar = new QWidget(parent);
    auto* h = new QHBoxLayout(bar);
    h->setContentsMargins(0, 0, 0, 0);

    auto makeButton = [&](QAction* a) {
        auto* b = new QToolButton(bar);
        b->setDefaultAction(a);
        b->setToolButtonStyle(Qt::ToolButtonIconOnly);
        b->setIconSize(QSize(24, 24));
        b->setAutoRaise(true);
        return b;
    };
    h->addWidget(makeButton(playAct_));
    h->addWidget(makeButton(pauseAct_));
    h->addWidget(makeButton(stopAct_));

    nowPlayingLabel_ = new QLabel(bar);
    nowPlayingLabel_->setMinimumWidth(160);
    nowPlayingLabel_->setMaximumWidth(260);
    nowPlayingLabel_->setTextInteractionFlags(Qt::NoTextInteraction);
    h->addWidget(nowPlayingLabel_);

    positionSlider_ = new QSlider(Qt::Horizontal, bar);
    positionSlider_->setRange(0, 0);
    h->addWidget(positionSlider_, 1);
    connect(positionSlider_, &QSlider::sliderMoved, this, [this](int v) { player_->setPosition(v); });
    connect(positionSlider_, &QSlider::sliderReleased, this, [this] { player_->setPosition(positionSlider_->value()); });

    timeLabel_ = new QLabel(QStringLiteral("0:00 / 0:00"), bar);
    timeLabel_->setMinimumWidth(95);
    timeLabel_->setAlignment(Qt::AlignCenter);
    h->addWidget(timeLabel_);

    auto* volIcon = new QLabel(bar);
    volIcon->setPixmap(icon("volume").pixmap(18, 18));
    h->addWidget(volIcon);
    volumeSlider_ = new QSlider(Qt::Horizontal, bar);
    volumeSlider_->setRange(0, 100);
    volumeSlider_->setValue(AppSettings::volume());
    volumeSlider_->setFixedWidth(110);
    volumeSlider_->setToolTip(tr("Lautstärke"));
    h->addWidget(volumeSlider_);
    connect(volumeSlider_, &QSlider::valueChanged, this, [this](int v) {
        audioOut_->setVolume(v / 100.0f);
        AppSettings::setVolume(v);
    });

    layout->addWidget(bar);
}

// ============================================================================ Liste
void MainWindow::onAddFiles() {
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("WAV-Dateien auswählen"), AppSettings::sourceDir(),
        tr("WAV-Audio (*.wav *.wave);;Alle Dateien (*)"));
    if (!files.isEmpty()) addPaths(files);
}

void MainWindow::onUrlsDropped(const QList<QUrl>& urls) {
    QStringList paths;
    for (const QUrl& u : urls)
        if (u.isLocalFile()) paths << u.toLocalFile();
    addPaths(paths);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* e) {
    if (e->mimeData()->hasUrls()) {
        e->acceptProposedAction();
        onUrlsDropped(e->mimeData()->urls());
    }
}

void MainWindow::collectWavFiles(const QStringList& paths, QStringList& out) const {
    for (const QString& p : paths) {
        const QFileInfo fi(p);
        if (fi.isDir()) {
            QDirIterator it(p, {QStringLiteral("*.wav"), QStringLiteral("*.wave")}, QDir::Files | QDir::Readable,
                            QDirIterator::Subdirectories);
            QStringList found;
            while (it.hasNext()) found << it.next();
            found.sort(Qt::CaseInsensitive);
            out << found;
        } else if (fi.isFile()) {
            out << p;
        }
    }
}

void MainWindow::addPaths(const QStringList& paths) {
    if (manager_->isRunning()) return;
    QStringList files;
    collectWavFiles(paths, files);

    int added = 0, dup = 0;
    QStringList invalid;
    table_->setUpdatesEnabled(false);
    for (const QString& f : files) {
        QString err;
        switch (addFile(f, &err)) {
        case AddResult::Added: ++added; break;
        case AddResult::Duplicate: ++dup; break;
        case AddResult::Invalid: invalid << QStringLiteral("%1 – %2").arg(QFileInfo(f).fileName(), err); break;
        }
    }
    table_->setUpdatesEnabled(true);

    if (!invalid.isEmpty()) {
        QString text = tr("%n Datei(en) konnten nicht übernommen werden:", nullptr, invalid.size()) + QLatin1String("\n\n");
        text += invalid.mid(0, 12).join(QLatin1Char('\n'));
        if (invalid.size() > 12) text += tr("\n… und %1 weitere").arg(invalid.size() - 12);
        QMessageBox::warning(this, tr("Nicht unterstützte Dateien"), text);
    }
    if (added > 0 || dup > 0)
        summaryLabel_->setText(tr("%1 Datei(en) hinzugefügt").arg(added) + (dup ? tr(", %1 bereits in der Liste").arg(dup) : QString()));
    updateUiState();
}

MainWindow::AddResult MainWindow::addFile(const QString& path, QString* error) {
    const QFileInfo fi(path);
    const QString abs = fi.absoluteFilePath();
    const QString key = pathKey(abs);
    if (knownPaths_.contains(key)) return AddResult::Duplicate;

    w2a::WavReader reader;
    std::string err;
    if (!reader.open(toFsPath(abs), err)) {
        if (error) *error = QString::fromStdString(err);
        return AddResult::Invalid;
    }
    const w2a::WavInfo& wi = reader.info();

    const int row = table_->rowCount();
    table_->insertRow(row);

    auto* name = new QTableWidgetItem(fi.fileName());
    name->setToolTip(QDir::toNativeSeparators(abs));
    name->setData(RolePath, abs);
    name->setData(RoleId, nextId_++);
    name->setData(RoleState, int(RowState::Pending));
    name->setData(RoleOutput, QString());
    table_->setItem(row, ColName, name);

    const QString chText = wi.channels == 1 ? tr("Mono") : (wi.channels == 2 ? tr("Stereo") : tr("%1 Kanäle").arg(wi.channels));
    const QString bits = wi.isFloat() ? tr("%1 Bit Float").arg(wi.bytesPerSample * 8) : tr("%1 Bit").arg(wi.bitsPerSample ? wi.bitsPerSample : wi.bytesPerSample * 8);
    const QString info = QStringLiteral("%1 · %2 kHz · %3 · %4")
                             .arg(formatTime(qint64(wi.durationSeconds() * 1000)),
                                  QString::number(wi.sampleRate / 1000.0, 'g', 6), bits, chText);
    auto* infoItem = new QTableWidgetItem(info);
    table_->setItem(row, ColInfo, infoItem);

    auto* bar = new QProgressBar;
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setTextVisible(true);
    table_->setCellWidget(row, ColProgress, bar);

    table_->setItem(row, ColStatus, new QTableWidgetItem(tr("Wartet")));
    knownPaths_.insert(key);
    return AddResult::Added;
}

int MainWindow::rowForId(int id) const {
    for (int r = 0; r < table_->rowCount(); ++r)
        if (table_->item(r, ColName)->data(RoleId).toInt() == id) return r;
    return -1;
}

QString MainWindow::rowPath(int row) const {
    return (row >= 0 && row < table_->rowCount()) ? table_->item(row, ColName)->data(RolePath).toString() : QString();
}

MainWindow::RowState MainWindow::rowState(int row) const {
    return static_cast<RowState>(table_->item(row, ColName)->data(RoleState).toInt());
}

void MainWindow::setRowState(int row, RowState st, const QString& text) {
    if (row < 0) return;
    table_->item(row, ColName)->setData(RoleState, int(st));
    table_->item(row, ColStatus)->setText(text);
    table_->item(row, ColStatus)->setToolTip(text);
}

void MainWindow::setRowProgress(int row, int percent) {
    if (auto* bar = qobject_cast<QProgressBar*>(table_->cellWidget(row, ColProgress))) bar->setValue(percent);
}

void MainWindow::removeRows(QList<int> rows) {
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int r : rows) {
        const QString p = rowPath(r);
        if (p == playingPath_ && player_->playbackState() != QMediaPlayer::StoppedState) player_->stop();
        knownPaths_.remove(pathKey(p));
        table_->removeRow(r);
    }
    updateUiState();
}

void MainWindow::onRemoveSelected() {
    if (manager_->isRunning()) return;
    QList<int> rows;
    for (const QModelIndex& idx : table_->selectionModel()->selectedRows()) rows << idx.row();
    removeRows(rows);
}

void MainWindow::onClearList() {
    if (manager_->isRunning()) return;
    player_->stop();
    table_->setRowCount(0);
    knownPaths_.clear();
    summaryLabel_->clear();
    overallBar_->setVisible(false);
    updateUiState();
}

// ============================================================================ Konvertierung
QString MainWindow::resolveOutputPath(const QString& inPath, QSet<QString>& reserved, bool* skip) const {
    const QFileInfo fi(inPath);
    const QString dir = AppSettings::targetSameAsSource() ? fi.absolutePath() : AppSettings::targetDir();
    const QString base = fi.completeBaseName();
    const OverwritePolicy policy = AppSettings::overwritePolicy();

    const QString ext = QString::fromLatin1(w2a::targetInfo(AppSettings::target()).extension);
    auto candidate = [&](int n) {
        return QDir(dir).filePath(n == 0 ? QStringLiteral("%1.%2").arg(base, ext)
                                         : QStringLiteral("%1_%2.%3").arg(base).arg(n).arg(ext));
    };

    QString out = candidate(0);
    *skip = false;
    if (policy == OverwritePolicy::Skip && QFile::exists(out)) {
        *skip = true;
        return out;
    }
    // Doppelte Zielnamen innerhalb desselben Durchlaufs (gleicher Dateiname aus
    // verschiedenen Ordnern) werden immer durchnummeriert. Bei „Neuen Namen
    // vergeben“ gilt das zusätzlich für bereits vorhandene Dateien.
    int n = 0;
    while (reserved.contains(pathKey(out)) || (policy == OverwritePolicy::Rename && QFile::exists(out)))
        out = candidate(++n);
    reserved.insert(pathKey(out));
    return out;
}

void MainWindow::onStartConversion() {
    if (manager_->isRunning()) return;

    if (!AppSettings::targetSameAsSource()) {
        if (!QDir().mkpath(AppSettings::targetDir())) {
            QMessageBox::warning(this, tr("Zielverzeichnis"),
                                 tr("Das Zielverzeichnis kann nicht angelegt werden:\n%1\n\nBitte in den Voreinstellungen korrigieren.")
                                     .arg(QDir::toNativeSeparators(AppSettings::targetDir())));
            return;
        }
    }

    const w2a::ConvertOptions opts = AppSettings::convertOptions();
    const QString sig = optionsSignature(opts);

    QList<ConvertManager::Job> jobs;
    QSet<QString> reserved;
    batchOk_ = batchFailed_ = batchCancelled_ = batchSkipped_ = 0;
    jobPercent_.clear();

    for (int r = 0; r < table_->rowCount(); ++r) {
        const RowState st = rowState(r);
        // Bereits mit denselben Einstellungen konvertiert -> überspringen (anderes Zielformat/andere Optionen: erneut)
        if (st == RowState::Done && table_->item(r, ColName)->data(RoleSig).toString() == sig) continue;

        bool skip = false;
        const QString in = rowPath(r);
        const QString out = resolveOutputPath(in, reserved, &skip);
        if (skip) {
            setRowState(r, RowState::Skipped, tr("Übersprungen (Zieldatei existiert bereits)"));
            setRowProgress(r, 0);
            ++batchSkipped_;
            continue;
        }
        ConvertManager::Job job;
        job.id = table_->item(r, ColName)->data(RoleId).toInt();
        job.inPath = in;
        job.outPath = out;
        job.options = opts;
        jobs << job;
        table_->item(r, ColName)->setData(RoleOutput, out);
        table_->item(r, ColName)->setData(RoleSig, sig);
        setRowState(r, RowState::Pending, tr("In Warteschlange"));
        setRowProgress(r, 0);
        jobPercent_.insert(job.id, 0);
    }

    if (jobs.isEmpty()) {
        summaryLabel_->setText(batchSkipped_ ? tr("%1 Datei(en) übersprungen").arg(batchSkipped_) : tr("Keine Dateien zu konvertieren"));
        updateUiState();
        return;
    }

    batchSize_ = jobs.size();
    overallBar_->setValue(0);
    overallBar_->setVisible(true);
    summaryLabel_->setText(tr("Konvertiere %1 Datei(en) …").arg(batchSize_));
    manager_->setMaxParallel(AppSettings::parallelJobs());
    manager_->start(jobs);
    updateUiState();
}

void MainWindow::onCancelConversion() {
    if (!manager_->isRunning()) return;
    manager_->cancelAll();
    summaryLabel_->setText(tr("Breche ab …"));
    cancelAct_->setEnabled(false);
}

void MainWindow::onJobStarted(int id) {
    const int row = rowForId(id);
    if (row < 0) return;
    setRowState(row, RowState::Running, tr("Konvertiere …"));
}

void MainWindow::onJobProgress(int id, int percent) {
    const int row = rowForId(id);
    if (row >= 0) setRowProgress(row, percent);
    jobPercent_[id] = percent;
    updateOverallProgress();
}

void MainWindow::onJobFinished(int id, int result, const QString& message) {
    const int row = rowForId(id);
    jobPercent_[id] = 100;
    switch (result) {
    case ConvertManager::Success:
        ++batchOk_;
        if (row >= 0) {
            setRowProgress(row, 100);
            setRowState(row, RowState::Done, tr("Fertig → %1").arg(QDir::toNativeSeparators(message)));
        }
        break;
    case ConvertManager::Cancelled:
        ++batchCancelled_;
        if (row >= 0) {
            setRowProgress(row, 0);
            setRowState(row, RowState::Cancelled, tr("Abgebrochen"));
        }
        break;
    default:
        ++batchFailed_;
        if (row >= 0) {
            setRowProgress(row, 0);
            setRowState(row, RowState::Failed, tr("Fehler: %1").arg(message));
        }
        break;
    }
    updateOverallProgress();
}

void MainWindow::updateOverallProgress() {
    if (batchSize_ <= 0) return;
    int sum = 0;
    for (int v : std::as_const(jobPercent_)) sum += v;
    overallBar_->setValue(sum / batchSize_);
}

void MainWindow::onAllFinished() {
    QStringList parts;
    if (batchOk_) parts << tr("%1 erfolgreich").arg(batchOk_);
    if (batchFailed_) parts << tr("%1 mit Fehler").arg(batchFailed_);
    if (batchCancelled_) parts << tr("%1 abgebrochen").arg(batchCancelled_);
    if (batchSkipped_) parts << tr("%1 übersprungen").arg(batchSkipped_);
    summaryLabel_->setText(tr("Fertig: %1").arg(parts.join(QStringLiteral(", "))));
    overallBar_->setVisible(false);
    updateUiState();
    QApplication::alert(this);
}

// ============================================================================ Wiedergabe
int MainWindow::currentRow() const {
    const auto sel = table_->selectionModel()->selectedRows();
    if (!sel.isEmpty()) return sel.first().row();
    return table_->currentRow();
}

void MainWindow::playPath(const QString& path) {
    if (!QFileInfo::exists(path)) {
        statusBar()->showMessage(tr("Datei nicht gefunden: %1").arg(QDir::toNativeSeparators(path)), 6000);
        return;
    }
    player_->stop();
    playingPath_ = path;
    nowPlayingLabel_->setText(QFileInfo(path).fileName());
    nowPlayingLabel_->setToolTip(QDir::toNativeSeparators(path));
    player_->setSource(QUrl::fromLocalFile(path));
    player_->play();
}

void MainWindow::playRow(int row, bool convertedFile) {
    if (row < 0 || row >= table_->rowCount()) return;
    QString path = rowPath(row);
    if (convertedFile) {
        path = table_->item(row, ColName)->data(RoleOutput).toString();
        if (path.isEmpty() || !QFile::exists(path)) return;
    }
    playPath(path);
}

void MainWindow::onPlayClicked() {
    const int row = currentRow();
    const QString sel = rowPath(row);
    // Pausiert und dieselbe (oder keine andere) Datei gewählt -> fortsetzen
    if (player_->playbackState() == QMediaPlayer::PausedState && (sel.isEmpty() || sel == playingPath_)) {
        player_->play();
        return;
    }
    if (row < 0) {
        statusBar()->showMessage(tr("Bitte zuerst eine Datei in der Liste auswählen."), 4000);
        return;
    }
    playPath(sel);
}

void MainWindow::onPauseClicked() { player_->pause(); }

void MainWindow::onStopClicked() { player_->stop(); }

void MainWindow::onPositionChanged(qint64 pos) {
    if (!positionSlider_->isSliderDown()) positionSlider_->setValue(int(pos));
    timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(pos), formatTime(player_->duration())));
}

void MainWindow::onDurationChanged(qint64 dur) {
    positionSlider_->setRange(0, int(dur));
    timeLabel_->setText(QStringLiteral("%1 / %2").arg(formatTime(player_->position()), formatTime(dur)));
}

void MainWindow::onPlaybackStateChanged(QMediaPlayer::PlaybackState) { updatePlaybackUi(); }

void MainWindow::updatePlaybackUi() {
    const auto st = player_->playbackState();
    playAct_->setEnabled(st != QMediaPlayer::PlayingState && table_->rowCount() > 0);
    pauseAct_->setEnabled(st == QMediaPlayer::PlayingState);
    stopAct_->setEnabled(st != QMediaPlayer::StoppedState);
    positionSlider_->setEnabled(st != QMediaPlayer::StoppedState);
    if (st == QMediaPlayer::StoppedState) {
        positionSlider_->setValue(0);
        timeLabel_->setText(QStringLiteral("0:00 / %1").arg(formatTime(player_->duration())));
        if (player_->mediaStatus() == QMediaPlayer::NoMedia) nowPlayingLabel_->clear();
    }
    // Wiedergabe-Symbol in der Liste
    for (int r = 0; r < table_->rowCount(); ++r) {
        QTableWidgetItem* it = table_->item(r, ColName);
        const bool active = st != QMediaPlayer::StoppedState && rowPath(r) == playingPath_;
        it->setIcon(active ? icon(st == QMediaPlayer::PlayingState ? "play" : "pause") : QIcon());
    }
}

QString MainWindow::formatTime(qint64 ms) {
    const qint64 total = ms / 1000;
    const qint64 h = total / 3600, m = (total % 3600) / 60, s = total % 60;
    return h > 0 ? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'))
                 : QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

// ============================================================================ Sonstiges
void MainWindow::onTableContextMenu(const QPoint& pos) {
    const int row = table_->rowAt(pos.y());
    if (row < 0) return;
    if (!table_->selectionModel()->isRowSelected(row, QModelIndex())) table_->selectRow(row);

    const bool busy = manager_->isRunning();
    const QString out = table_->item(row, ColName)->data(RoleOutput).toString();
    const bool haveFile = rowState(row) == RowState::Done && !out.isEmpty() && QFile::exists(out);

    QMenu menu(this);
    QAction* a1 = menu.addAction(icon("play"), tr("Original (WAV) abspielen"));
    connect(a1, &QAction::triggered, this, [this, row] { playRow(row, false); });
    QAction* a2 = menu.addAction(icon("play"), tr("Erzeugte Datei abspielen (falls vom System unterstützt)"));
    a2->setEnabled(haveFile);
    connect(a2, &QAction::triggered, this, [this, row] { playRow(row, true); });
    menu.addSeparator();
    QAction* a3 = menu.addAction(icon("folder"), tr("Zielordner öffnen"));
    a3->setEnabled(haveFile);
    connect(a3, &QAction::triggered, this, [out] {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(out).absolutePath()));
    });
    QAction* a4 = menu.addAction(icon("convert"), tr("Erneut konvertieren"));
    a4->setEnabled(!busy && rowState(row) != RowState::Pending);
    connect(a4, &QAction::triggered, this, [this] {
        for (const QModelIndex& idx : table_->selectionModel()->selectedRows()) {
            setRowState(idx.row(), RowState::Pending, tr("Wartet"));
            setRowProgress(idx.row(), 0);
        }
        updateUiState();
    });
    menu.addSeparator();
    QAction* a5 = menu.addAction(icon("remove"), tr("Aus Liste entfernen"));
    a5->setEnabled(!busy);
    connect(a5, &QAction::triggered, this, &MainWindow::onRemoveSelected);

    menu.exec(table_->viewport()->mapToGlobal(pos));
}

void MainWindow::updateUiState() {
    const bool busy = manager_->isRunning();
    const bool has = table_->rowCount() > 0;
    addAct_->setEnabled(!busy);
    removeAct_->setEnabled(!busy && table_->selectionModel()->hasSelection());
    clearAct_->setEnabled(!busy && has);
    convertAct_->setEnabled(!busy && has);
    cancelAct_->setEnabled(busy);
    settingsAct_->setEnabled(!busy);
    if (targetCombo_) targetCombo_->setEnabled(!busy);
    updatePlaybackUi();
}

void MainWindow::refreshStatusInfo() {
    QString fmt = targetLabel(AppSettings::target());
    if (AppSettings::target() == w2a::Target::Svx8 && AppSettings::fibonacci()) fmt += tr(", Fibonacci-Delta");
    const QString dir = AppSettings::targetSameAsSource() ? tr("Ordner der Quelldatei")
                                                          : QDir::toNativeSeparators(AppSettings::targetDir());
    settingsLabel_->setText(tr("Zielformat: %1   |   Ziel: %2").arg(fmt, dir));
}

void MainWindow::onTargetChanged() {
    const w2a::Target t = w2a::targetFromKey(targetCombo_->currentData().toString().toStdString());
    AppSettings::setTarget(t);               // gewählten Konverter sofort merken
    targetCombo_->setToolTip(targetToolTip(t));
    refreshStatusInfo();
}

void MainWindow::saveWindowLayout() {
    AppSettings::setWindowGeometry(saveGeometry());   // Größe, Position, Maximiert-Status
    AppSettings::setWindowState(saveState());
}

void MainWindow::moveEvent(QMoveEvent* e) {
    QMainWindow::moveEvent(e);
    if (layoutTimer_) layoutTimer_->start();
}

void MainWindow::resizeEvent(QResizeEvent* e) {
    QMainWindow::resizeEvent(e);
    if (layoutTimer_) layoutTimer_->start();
}

void MainWindow::onSettings() {
    SettingsDialog dlg(this);
    if (dlg.exec() == QDialog::Accepted) refreshStatusInfo();
}

void MainWindow::onAbout() {
    AboutDialog dlg(this);
    dlg.exec();
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (manager_->isRunning()) {
        const auto r = QMessageBox::question(this, tr("Konvertierung läuft"),
                                             tr("Es läuft noch eine Konvertierung. Abbrechen und beenden?"));
        if (r != QMessageBox::Yes) {
            e->ignore();
            return;
        }
        manager_->cancelAll();
    }
    player_->stop();
    saveWindowLayout();
    AppSettings::setTarget(w2a::targetFromKey(targetCombo_->currentData().toString().toStdString()));
    e->accept();
}
