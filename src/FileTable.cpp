// SPDX-License-Identifier: MIT
#include "FileTable.h"

#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>

FileTable::FileTable(QWidget* parent) : QTableWidget(parent) {
    setAcceptDrops(true);
    viewport()->setAcceptDrops(true);
}

void FileTable::dragEnterEvent(QDragEnterEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
    else e->ignore();
}

void FileTable::dragMoveEvent(QDragMoveEvent* e) {
    if (e->mimeData()->hasUrls()) e->acceptProposedAction();
    else e->ignore();
}

void FileTable::dropEvent(QDropEvent* e) {
    if (!e->mimeData()->hasUrls()) {
        e->ignore();
        return;
    }
    e->acceptProposedAction();
    emit urlsDropped(e->mimeData()->urls());
}

void FileTable::paintEvent(QPaintEvent* e) {
    QTableWidget::paintEvent(e);
    if (rowCount() == 0 && !hint_.isEmpty()) {
        QPainter p(viewport());
        p.setPen(palette().color(QPalette::Disabled, QPalette::Text));
        QFont f = font();
        f.setPointSizeF(f.pointSizeF() * 1.25);
        p.setFont(f);
        p.drawText(viewport()->rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap, hint_);
    }
}
