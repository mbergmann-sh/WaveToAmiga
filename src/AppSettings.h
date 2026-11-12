// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#pragma once

#include "core/Converter.h"

#include <QByteArray>
#include <QString>

enum class OverwritePolicy { Rename = 0, Overwrite = 1, Skip = 2 };

// Anzeigetexte der Zielformate (GUI, übersetzbar)
QString targetLabel(w2a::Target t);        // z. B. "IFF-8SVX (8 Bit)"
QString targetToolTip(w2a::Target t);      // längere Beschreibung
QString maudCodingLabel(w2a::MaudCoding c);
QString maudCodingKey(w2a::MaudCoding c);
w2a::MaudCoding maudCodingFromKey(const QString& key);

// Persistente Voreinstellungen (QSettings). Alle Funktionen werden nur aus dem GUI-Thread aufgerufen.
namespace AppSettings {

QString defaultSourceDir();
QString defaultTargetDir();

QString sourceDir();
void setSourceDir(const QString& dir);
QString targetDir();
void setTargetDir(const QString& dir);
bool targetSameAsSource();
void setTargetSameAsSource(bool on);

// ---- Zielformat ("Konverter") und seine Optionen -------------------------------------------------------
w2a::Target target();                      // zuletzt gewähltes Zielformat (Standard: IFF-8SVX)
void setTarget(w2a::Target t);
bool fibonacci();                          // 8SVX: Fibonacci-Delta-Kompression
void setFibonacci(bool on);
int aiffBits();                            // 0 = wie Quelle, sonst 8/16/24/32
void setAiffBits(int bits);
w2a::MaudCoding maudCoding();
void setMaudCoding(w2a::MaudCoding c);
int targetRate();                          // 0 = wie Quelle
void setTargetRate(int hz);
bool forceMono();
void setForceMono(bool on);
w2a::ConvertOptions convertOptions();      // alles zusammengefasst für den Konverter

// ---- Verarbeitung --------------------------------------------------------------------------------------
int parallelJobs();                        // Standard: halbe Kernanzahl (1…4)
void setParallelJobs(int n);
int defaultParallelJobs();
OverwritePolicy overwritePolicy();         // Standard: umbenennen
void setOverwritePolicy(OverwritePolicy p);

// ---- Fenster / Player ----------------------------------------------------------------------------------
int volume();                              // 0…100
void setVolume(int v);
QByteArray windowGeometry();               // Größe + Position (+ Maximiert-Status, Bildschirm)
void setWindowGeometry(const QByteArray& g);
QByteArray windowState();                  // Werkzeugleisten-Zustand
void setWindowState(const QByteArray& s);

void resetToDefaults();                    // Verzeichnisse, Zielformat-Optionen, Verarbeitung (nicht Fenster/Lautstärke/Zielformat)

} // namespace AppSettings
