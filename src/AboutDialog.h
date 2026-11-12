// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#pragma once

#include <QDialog>

// „Über“-Dialog: Version, Copyright, MIT-Lizenz (voller Text) und Drittanbieter-Hinweise.
class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
};
