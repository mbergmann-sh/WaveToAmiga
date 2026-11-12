// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
// WAV -> Amiga-Soundformate (8SVX, AIFF, 16SV, MAUD) mit der eingebundenen libsox. Reiner C++17-Kern ohne Qt.
#pragma once

#include "Targets.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace w2a {

enum class MaudCoding { Pcm16, Pcm8, ULaw, ALaw };

struct ConvertOptions {
    Target target = Target::Svx8;
    bool fibonacci = false;          // nur 8SVX: Fibonacci-Delta-Kompression (sCompression = 1)
    int aiffBits = 0;                // nur AIFF: 0 = wie Quelle (8/16/24/32), sonst 8, 16, 24 oder 32
    MaudCoding maud = MaudCoding::Pcm16;  // nur MAUD
    int targetRate = 0;              // 0 = wie Quelle; sonst Zielabtastrate in Hz (wird per Resampler erzeugt)
    bool forceMono = false;          // alle Kanäle zu einem mischen
};

struct ConvertResult {
    bool ok = false;
    bool cancelled = false;
    std::string error;               // deutsche Fehlermeldung, wenn !ok
    uint64_t outBytes = 0;
    int outRate = 0;                 // tatsächlich geschriebene Abtastrate
    int outChannels = 0;
};

using ProgressFn = std::function<void(int percent)>;

// Initialisiert libsox (einmalig, thread-sicher). Wird von convertWav() bei Bedarf selbst aufgerufen.
bool initSox();

// Konvertiert `in` (WAV) nach `out`. `cancel` wird regelmäßig geprüft. Bei Fehler oder Abbruch wird die
// (Teil-)Ausgabedatei wieder gelöscht.
ConvertResult convertWav(const std::filesystem::path& in, const std::filesystem::path& out,
                         const ConvertOptions& options, const std::atomic<bool>& cancel,
                         const ProgressFn& progress = {});

// Version der eingebundenen libsox, z. B. "14.4.2".
std::string soxVersion();

} // namespace w2a
