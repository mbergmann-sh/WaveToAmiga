// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
// Streaming-Resampler (gefensterte Sinc-Interpolation, Blackman-Fenster) für beliebige Ratenverhältnisse.
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace w2a {

class Resampler {
public:
    Resampler(double rateIn, double rateOut, int channels);

    // Hängt interleaved Float-Frames an und liefert die bereits berechneten Ausgabe-Frames (append an `out`).
    void process(const float* in, size_t frames, std::vector<float>& out);
    // Am Ende des Signals aufrufen: liefert die restlichen Frames.
    void flush(std::vector<float>& out);

    int64_t totalOutputFrames() const;   // nach flush(): Anzahl aller erzeugten Frames

private:
    void run(std::vector<float>& out, bool final);
    float kernel(double distance) const;

    int ch_;
    double ratio_;          // rateIn / rateOut
    double cutoff_;         // normierte Grenzfrequenz (0..1, bezogen auf die Eingangs-Nyquist)
    double halfWidth_;      // halbe Filterlänge in Eingangs-Samples
    static constexpr int kTableRes = 256;  // Stützstellen pro Eingangs-Sample
    std::vector<float> table_;

    std::vector<float> buf_;  // interleaved, beginnt bei absolutem Frame base_
    int64_t base_ = 0;
    int64_t totalIn_ = 0;
    int64_t outCount_ = 0;
};

} // namespace w2a
