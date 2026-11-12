// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QTableWidget>
#include <QUrl>

// Tabelle, die per Drag & Drop Dateien/Ordner entgegennimmt und bei leerem
// Inhalt einen Hinweistext anzeigt.
class FileTable : public QTableWidget {
    Q_OBJECT
public:
    explicit FileTable(QWidget* parent = nullptr);
    void setHintText(const QString& text) { hint_ = text; viewport()->update(); }

signals:
    void urlsDropped(const QList<QUrl>& urls);

protected:
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dragMoveEvent(QDragMoveEvent* e) override;
    void dropEvent(QDropEvent* e) override;
    void paintEvent(QPaintEvent* e) override;

private:
    QString hint_;
};
