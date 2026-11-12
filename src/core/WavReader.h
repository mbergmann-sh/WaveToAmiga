// SPDX-License-Identifier: MIT
// Liest WAV-Dateien (RIFF/RF64, PCM 8/16/24/32 Bit, IEEE-Float 32/64 Bit,
// WAVE_FORMAT_EXTENSIBLE) und liefert die Samples als interleaved int32 (linksbündig, Vollaussteuerung
// = ±2^31). So bleiben 24- und 32-Bit-Quellen für AIFF verlustfrei.
#pragma once

#include "File.h"

#include <cstdint>
#include <string>
#include <vector>

namespace w2a {

struct WavInfo {
    uint16_t formatTag = 0;       // 1 = PCM, 3 = IEEE-Float
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;   // laut Header
    uint16_t bytesPerSample = 0;  // Containergröße pro Sample
    uint16_t blockAlign = 0;      // Bytes pro Frame (alle Kanäle)
    uint64_t dataOffset = 0;
    uint64_t dataBytes = 0;
    uint64_t totalFrames = 0;

    double durationSeconds() const {
        return sampleRate ? static_cast<double>(totalFrames) / sampleRate : 0.0;
    }
    bool isFloat() const { return formatTag == 3; }
};

class WavReader {
public:
    // Öffnet die Datei und parst den Header. Bei Fehlern: false + Meldung (deutsch).
    bool open(const std::filesystem::path& path, std::string& error);

    const WavInfo& info() const { return info_; }

    // Höchstzahl der gelieferten Kanäle (Standard 2). Hat die Datei mehr Kanäle, werden nur die
    // ersten maxChannels ausgegeben.
    void setMaxChannels(int n) { maxChannels_ = n < 1 ? 1 : n; }
    int outChannels() const { return info_.channels < maxChannels_ ? info_.channels : maxChannels_; }

    // Liest bis zu maxFrames Frames, konvertiert nach int32 (interleaved, outChannels() Kanäle).
    // Rückgabe: tatsächlich gelesene Frames (0 = Ende).
    size_t readFrames(int32_t* out, size_t maxFrames);

private:
    File file_;
    WavInfo info_;
    uint64_t framesRead_ = 0;
    int maxChannels_ = 2;
    std::vector<uint8_t> raw_;
};

} // namespace w2a
