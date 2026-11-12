// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
// Die Amiga-Zielformate von WaveToAmiga (Qt-freie Beschreibung; Anzeigetexte stehen in der GUI).
#pragma once

#include <string>
#include <vector>

namespace w2a {

enum class Target {
    Svx8,   // IFF-8SVX (SoX 8svx.c), optional Fibonacci-Delta-Kompression
    Aiff,   // AIFF (SoX aiff.c)
    Svx16,  // IFF-16SV (16 Bit, Aufbau wie 8SVX)
    Maud,   // IFF-MAUD (SoX maud.c)
};

struct TargetInfo {
    Target id;
    const char* key;        // stabiler Schlüssel für die Einstellungen
    const char* extension;  // Dateiendung ohne Punkt
    int maxChannels;        // wie viele Kanäle der Handler schreiben kann
};

const std::vector<TargetInfo>& allTargets();
const TargetInfo& targetInfo(Target t);
Target targetFromKey(const std::string& key, Target fallback = Target::Svx8);

} // namespace w2a
