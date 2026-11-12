// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "SettingsDialog.h"

#include "AppSettings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Voreinstellungen"));
    setModal(true);

    // ---- Verzeichnisse -----------------------------------------------------
    auto* pathsBox = new QGroupBox(tr("Verzeichnisse"), this);
    auto* pathsForm = new QFormLayout(pathsBox);

    sourceEdit_ = new QLineEdit(pathsBox);
    auto* sourceBtn = new QPushButton(QIcon(QStringLiteral(":/icons/folder.png")), tr("Durchsuchen …"), pathsBox);
    auto* sourceRow = new QHBoxLayout;
    sourceRow->addWidget(sourceEdit_, 1);
    sourceRow->addWidget(sourceBtn);
    pathsForm->addRow(tr("Quellverzeichnis:"), sourceRow);

    sameDirCheck_ = new QCheckBox(tr("Zieldatei im Ordner der jeweiligen Quelldatei speichern"), pathsBox);
    pathsForm->addRow(QString(), sameDirCheck_);

    targetEdit_ = new QLineEdit(pathsBox);
    auto* targetBtn = new QPushButton(QIcon(QStringLiteral(":/icons/folder.png")), tr("Durchsuchen …"), pathsBox);
    auto* targetRow = new QHBoxLayout;
    targetRow->addWidget(targetEdit_, 1);
    targetRow->addWidget(targetBtn);
    pathsForm->addRow(tr("Zielverzeichnis:"), targetRow);

    auto* pathsHint = new QLabel(tr("Das Quellverzeichnis ist der Startordner des Datei-Dialogs. "
                                    "Fehlende Zielordner werden automatisch angelegt."), pathsBox);
    pathsHint->setWordWrap(true);
    pathsHint->setEnabled(false);
    pathsForm->addRow(pathsHint);

    // ---- Zielformat-Optionen -----------------------------------------------
    auto* fmtBox = new QGroupBox(tr("Zielformat-Optionen"), this);
    auto* fmtForm = new QFormLayout(fmtBox);

    fibCheck_ = new QCheckBox(tr("Fibonacci-Delta-Kompression (4 Bit je Sample, halbe Dateigröße, verlustbehaftet)"), fmtBox);
    fmtForm->addRow(tr("IFF-8SVX:"), fibCheck_);

    aiffBitsCombo_ = new QComboBox(fmtBox);
    aiffBitsCombo_->addItem(tr("wie Quelldatei"), 0);
    for (int b : {8, 16, 24, 32}) aiffBitsCombo_->addItem(tr("%1 Bit").arg(b), b);
    fmtForm->addRow(tr("AIFF – Bittiefe:"), aiffBitsCombo_);

    maudCombo_ = new QComboBox(fmtBox);
    for (auto c : {w2a::MaudCoding::Pcm16, w2a::MaudCoding::Pcm8, w2a::MaudCoding::ULaw, w2a::MaudCoding::ALaw})
        maudCombo_->addItem(maudCodingLabel(c), maudCodingKey(c));
    fmtForm->addRow(tr("IFF-MAUD – Kodierung:"), maudCombo_);

    rateSpin_ = new QSpinBox(fmtBox);
    rateSpin_->setRange(0, 192000);
    rateSpin_->setSingleStep(1000);
    rateSpin_->setSuffix(tr(" Hz"));
    rateSpin_->setSpecialValueText(tr("wie Quelldatei"));
    rateSpin_->setToolTip(tr("Zielabtastrate, z. B. 8363, 11025 oder 22050 Hz für klassische Amiga-Samples.\n"
                             "IFF-8SVX/16SV erlauben höchstens 65535 Hz (höhere Raten werden automatisch halbiert)."));
    fmtForm->addRow(tr("Abtastrate (alle Formate):"), rateSpin_);

    monoCheck_ = new QCheckBox(tr("Mehrkanal-Dateien zu Mono mischen"), fmtBox);
    fmtForm->addRow(tr("Kanäle (alle Formate):"), monoCheck_);

    auto* fmtHint = new QLabel(tr("Das Zielformat selbst wählen Sie in der Werkzeugleiste. 8SVX und 16SV schreiben bis zu 2 Kanäle "
                                  "(4 bei 4-Kanal-Quellen), MAUD 2, AIFF bis zu 8 Kanäle."), fmtBox);
    fmtHint->setWordWrap(true);
    fmtHint->setEnabled(false);
    fmtForm->addRow(fmtHint);

    // ---- Verarbeitung ------------------------------------------------------
    auto* procBox = new QGroupBox(tr("Verarbeitung"), this);
    auto* procForm = new QFormLayout(procBox);

    overwriteCombo_ = new QComboBox(procBox);
    overwriteCombo_->addItem(tr("Neuen Namen vergeben (name_1.endung)"), int(OverwritePolicy::Rename));
    overwriteCombo_->addItem(tr("Vorhandene Datei überschreiben"), int(OverwritePolicy::Overwrite));
    overwriteCombo_->addItem(tr("Datei überspringen"), int(OverwritePolicy::Skip));
    procForm->addRow(tr("Wenn die Zieldatei schon existiert:"), overwriteCombo_);

    jobsSpin_ = new QSpinBox(procBox);
    jobsSpin_->setRange(1, qMax(1, QThread::idealThreadCount()));
    jobsSpin_->setToolTip(tr("Anzahl gleichzeitig konvertierter Dateien"));
    procForm->addRow(tr("Parallele Konvertierungen:"), jobsSpin_);

    // ---- Buttons -----------------------------------------------------------
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("OK"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Abbrechen"));
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText(tr("Standard"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(pathsBox);
    layout->addWidget(fmtBox);
    layout->addWidget(procBox);
    layout->addWidget(buttons);

    connect(sourceBtn, &QPushButton::clicked, this, &SettingsDialog::browseSource);
    connect(targetBtn, &QPushButton::clicked, this, &SettingsDialog::browseTarget);
    connect(sameDirCheck_, &QCheckBox::toggled, this, &SettingsDialog::updateEnabledState);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, &SettingsDialog::restoreDefaults);

    loadFromSettings();
    resize(680, sizeHint().height());
}

void SettingsDialog::loadFromSettings() {
    sourceEdit_->setText(QDir::toNativeSeparators(AppSettings::sourceDir()));
    targetEdit_->setText(QDir::toNativeSeparators(AppSettings::targetDir()));
    sameDirCheck_->setChecked(AppSettings::targetSameAsSource());
    fibCheck_->setChecked(AppSettings::fibonacci());
    aiffBitsCombo_->setCurrentIndex(qMax(0, aiffBitsCombo_->findData(AppSettings::aiffBits())));
    maudCombo_->setCurrentIndex(qMax(0, maudCombo_->findData(maudCodingKey(AppSettings::maudCoding()))));
    rateSpin_->setValue(AppSettings::targetRate());
    monoCheck_->setChecked(AppSettings::forceMono());
    overwriteCombo_->setCurrentIndex(qMax(0, overwriteCombo_->findData(int(AppSettings::overwritePolicy()))));
    jobsSpin_->setValue(AppSettings::parallelJobs());
    updateEnabledState();
}

void SettingsDialog::updateEnabledState() { targetEdit_->setEnabled(!sameDirCheck_->isChecked()); }

void SettingsDialog::browseSource() {
    const QString d = QFileDialog::getExistingDirectory(this, tr("Quellverzeichnis wählen"), sourceEdit_->text());
    if (!d.isEmpty()) sourceEdit_->setText(QDir::toNativeSeparators(d));
}

void SettingsDialog::browseTarget() {
    const QString d = QFileDialog::getExistingDirectory(this, tr("Zielverzeichnis wählen"), targetEdit_->text());
    if (!d.isEmpty()) targetEdit_->setText(QDir::toNativeSeparators(d));
}

void SettingsDialog::restoreDefaults() {
    sourceEdit_->setText(QDir::toNativeSeparators(AppSettings::defaultSourceDir()));
    targetEdit_->setText(QDir::toNativeSeparators(AppSettings::defaultTargetDir()));
    sameDirCheck_->setChecked(false);
    fibCheck_->setChecked(false);
    aiffBitsCombo_->setCurrentIndex(0);
    maudCombo_->setCurrentIndex(0);
    rateSpin_->setValue(0);
    monoCheck_->setChecked(false);
    overwriteCombo_->setCurrentIndex(qMax(0, overwriteCombo_->findData(int(OverwritePolicy::Rename))));
    jobsSpin_->setValue(AppSettings::defaultParallelJobs());
}

void SettingsDialog::accept() {
    const QString src = QDir::fromNativeSeparators(sourceEdit_->text().trimmed());
    const QString dst = QDir::fromNativeSeparators(targetEdit_->text().trimmed());

    if (!src.isEmpty() && !QDir(src).exists()) {
        QMessageBox::warning(this, tr("Quellverzeichnis"), tr("Das Quellverzeichnis existiert nicht:\n%1").arg(src));
        return;
    }
    if (!sameDirCheck_->isChecked()) {
        if (dst.isEmpty() || !QDir().mkpath(dst)) {
            QMessageBox::warning(this, tr("Zielverzeichnis"),
                                 tr("Das Zielverzeichnis kann nicht angelegt werden:\n%1").arg(dst));
            return;
        }
    }

    AppSettings::setSourceDir(src);
    if (!dst.isEmpty()) AppSettings::setTargetDir(dst);
    AppSettings::setTargetSameAsSource(sameDirCheck_->isChecked());
    AppSettings::setFibonacci(fibCheck_->isChecked());
    AppSettings::setAiffBits(aiffBitsCombo_->currentData().toInt());
    AppSettings::setMaudCoding(maudCodingFromKey(maudCombo_->currentData().toString()));
    AppSettings::setTargetRate(rateSpin_->value());
    AppSettings::setForceMono(monoCheck_->isChecked());
    AppSettings::setOverwritePolicy(static_cast<OverwritePolicy>(overwriteCombo_->currentData().toInt()));
    AppSettings::setParallelJobs(jobsSpin_->value());
    QDialog::accept();
}
