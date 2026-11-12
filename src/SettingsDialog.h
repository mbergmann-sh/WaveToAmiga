// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void accept() override;

private slots:
    void browseSource();
    void browseTarget();
    void restoreDefaults();
    void updateEnabledState();

private:
    void loadFromSettings();

    QLineEdit* sourceEdit_ = nullptr;
    QLineEdit* targetEdit_ = nullptr;
    QCheckBox* sameDirCheck_ = nullptr;

    QCheckBox* fibCheck_ = nullptr;      // 8SVX: Fibonacci-Delta
    QComboBox* aiffBitsCombo_ = nullptr; // AIFF: Bittiefe
    QComboBox* maudCombo_ = nullptr;     // MAUD: Kodierung
    QSpinBox* rateSpin_ = nullptr;       // Zielabtastrate (0 = wie Quelle)
    QCheckBox* monoCheck_ = nullptr;     // zu Mono mischen

    QComboBox* overwriteCombo_ = nullptr;
    QSpinBox* jobsSpin_ = nullptr;
};
