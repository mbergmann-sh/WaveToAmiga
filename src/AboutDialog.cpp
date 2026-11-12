// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "AboutDialog.h"

#include "core/Converter.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace {
QString readResource(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QObject::tr("(Text nicht verfügbar)");
    return QString::fromUtf8(f.readAll());
}

QPlainTextEdit* makeTextView(const QString& text, QWidget* parent) {
    auto* e = new QPlainTextEdit(parent);
    e->setReadOnly(true);
    e->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    e->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    e->setPlainText(text);
    return e;
}
} // namespace

AboutDialog::AboutDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Über WaveToAmiga"));
    setModal(true);

    // ---- Kopfbereich: Logo + Titel + Copyright + Lizenz ----------------------------
    auto* logo = new QLabel(this);
    logo->setPixmap(QPixmap(QStringLiteral(":/icons/app_256.png"))
                        .scaled(112, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    logo->setAlignment(Qt::AlignTop);

    auto* head = new QLabel(this);
    head->setTextFormat(Qt::RichText);
    head->setWordWrap(true);
    head->setOpenExternalLinks(true);                       // Link im Standardbrowser öffnen
    head->setTextInteractionFlags(Qt::TextBrowserInteraction);
    head->setText(
        tr("<div style='font-size:22pt; font-weight:600;'>WaveToAmiga</div>"
           "<div style='font-size:10pt;'>Version %1</div>"
           "<p style='font-size:11pt; margin-top:10px;'>WaveToAmiga - ein Tool zum Konvertieren von WAV-Dateien "
           "in gängige Amiga-Soundformate</p>"
           "<p style='font-size:10pt;'>Autor: Michael Bergmann<br>"
           "Website: <a href=\"https://portfolio.mbergmann-sh.de\">https://portfolio.mbergmann-sh.de</a></p>"
           "<p style='font-size:10pt;'><b>© Michael Bergmann 2026</b> &nbsp;·&nbsp; Lizenz: <b>MIT-Lizenz</b> "
           "(siehe Reiter „MIT-Lizenz“)</p>")
            .arg(QApplication::applicationVersion()));

    auto* headRow = new QHBoxLayout;
    headRow->setSpacing(18);
    headRow->addWidget(logo);
    headRow->addWidget(head, 1);

    // ---- Reiter -------------------------------------------------------------------
    auto* about = new QTextBrowser(this);
    about->setOpenExternalLinks(true);
    about->setHtml(
        tr("<p><b>Zielformate</b></p>"
           "<ul>"
           "<li><b>IFF-8SVX</b> – klassisches 8-Bit-Amiga-Sample, unkomprimiert oder mit Fibonacci-Delta-Kompression</li>"
           "<li><b>AIFF</b> – Audio Interchange File Format (IFF-Container), 8 bis 32 Bit, mehrere Kanäle</li>"
           "<li><b>IFF-16SV</b> – 16-Bit-Variante von 8SVX</li>"
           "<li><b>IFF-MAUD</b> – 16 Bit, 8 Bit, µ-law oder A-law</li>"
           "</ul>"
           "<p>Dateien lassen sich per Mehrfachauswahl oder Drag&nbsp;&amp;&nbsp;Drop hinzufügen und vor der "
           "Konvertierung im eingebauten Player probehören.</p>"
           "<p><b>Verwendete Komponenten</b></p>"
           "<ul>"
           "<li><b>SoX / libsox %1</b> – Format-Handler für 8SVX, AIFF und MAUD (LGPL), fest eingebunden; "
           "Fibonacci-8SVX und 16SV sind Ergänzungen im Stil der SoX-Handler</li>"
           "<li><b>Qt %2</b> – Oberfläche und Audiowiedergabe (LGPL v3)</li>"
           "</ul>"
           "<p><b>Lizenz</b><br>"
           "Dieses Programm ist freie Software unter der <b>MIT-Lizenz</b>: Sie dürfen es frei nutzen, "
           "kopieren, ändern und weitergeben, sofern der Copyright-Hinweis „© Michael Bergmann 2026“ "
           "und der Lizenztext erhalten bleiben. Es wird <b>ohne jede Gewährleistung</b> geliefert.</p>"
           "<p>Die Lizenzen der Drittanbieter-Komponenten finden Sie im gleichnamigen Reiter.</p>")
            .arg(QString::fromStdString(w2a::soxVersion()), QString::fromLatin1(qVersion())));

    auto* tabs = new QTabWidget(this);
    tabs->addTab(about, tr("Über"));
    tabs->addTab(makeTextView(readResource(QStringLiteral(":/legal/LICENSE.txt")), this), tr("MIT-Lizenz"));
    tabs->addTab(makeTextView(readResource(QStringLiteral(":/legal/THIRD_PARTY_LICENSES.txt")), this),
                 tr("Drittanbieter"));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Schließen"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(headRow);
    layout->addWidget(tabs, 1);
    layout->addWidget(buttons);

    resize(680, 600);
}
