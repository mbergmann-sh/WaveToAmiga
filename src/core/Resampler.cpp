// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "Resampler.h"

#include <algorithm>
#include <cmath>

namespace w2a {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kZeroCrossings = 16;   // Filterqualität (Nulldurchgänge je Seite)
}

Resampler::Resampler(double rateIn, double rateOut, int channels)
    : ch_(channels < 1 ? 1 : channels), ratio_(rateIn / rateOut) {
    // Bei Dezimierung (Ausgangsrate < Eingangsrate) Tiefpass auf die neue Nyquist-Frequenz
    // (mit 4 % Sicherheitsabstand), sonst nur leichter Abstand zur Nyquist-Frequenz.
    cutoff_ = std::min(1.0, rateOut / rateIn) * 0.96;
    halfWidth_ = kZeroCrossings / cutoff_;
    const int n = static_cast<int>(std::ceil(halfWidth_ * kTableRes)) + 2;
    table_.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        const double d = static_cast<double>(i) / kTableRes;
        double v = 0.0;
        if (d < halfWidth_) {
            const double x = cutoff_ * d;
            const double sinc = (x < 1e-12) ? 1.0 : std::sin(kPi * x) / (kPi * x);
            const double u = d / halfWidth_;
            const double w = 0.42 + 0.5 * std::cos(kPi * u) + 0.08 * std::cos(2.0 * kPi * u);  // Blackman
            v = cutoff_ * sinc * w;
        }
        table_[static_cast<size_t>(i)] = static_cast<float>(v);
    }
}

float Resampler::kernel(double distance) const {
    const double p = std::fabs(distance) * kTableRes;
    const size_t i = static_cast<size_t>(p);
    if (i + 1 >= table_.size()) return 0.0f;
    const float frac = static_cast<float>(p - static_cast<double>(i));
    return table_[i] + (table_[i + 1] - table_[i]) * frac;
}

void Resampler::process(const float* in, size_t frames, std::vector<float>& out) {
    buf_.insert(buf_.end(), in, in + frames * static_cast<size_t>(ch_));
    totalIn_ += static_cast<int64_t>(frames);
    run(out, false);
}

void Resampler::flush(std::vector<float>& out) { run(out, true); }

int64_t Resampler::totalOutputFrames() const { return outCount_; }

void Resampler::run(std::vector<float>& out, bool final) {
    const int64_t avail = base_ + static_cast<int64_t>(buf_.size() / static_cast<size_t>(ch_));  // erster nicht vorhandener Frame
    const int64_t outTotal = static_cast<int64_t>(std::floor(static_cast<double>(totalIn_) / ratio_ + 0.5));
    std::vector<double> acc(static_cast<size_t>(ch_));

    while (true) {
        if (final && outCount_ >= outTotal) break;
        const double t = static_cast<double>(outCount_) * ratio_;
        const int64_t last = static_cast<int64_t>(std::floor(t + halfWidth_));
        if (!final && last >= avail) break;                      // noch nicht genug Eingangsdaten

        const int64_t first = std::max<int64_t>(static_cast<int64_t>(std::ceil(t - halfWidth_)), std::max<int64_t>(base_, 0));
        const int64_t stop = std::min<int64_t>(last, avail - 1);
        std::fill(acc.begin(), acc.end(), 0.0);
        for (int64_t k = first; k <= stop; ++k) {
            const float w = kernel(t - static_cast<double>(k));
            const float* frame = &buf_[static_cast<size_t>(k - base_) * static_cast<size_t>(ch_)];
            for (int c = 0; c < ch_; ++c) acc[static_cast<size_t>(c)] += static_cast<double>(w) * frame[c];
        }
        for (int c = 0; c < ch_; ++c) out.push_back(static_cast<float>(acc[static_cast<size_t>(c)]));
        ++outCount_;
    }

    // verbrauchte Eingangs-Frames verwerfen (nur ab und zu, damit das Löschen billig bleibt)
    const int64_t keepFrom = static_cast<int64_t>(std::floor(static_cast<double>(outCount_) * ratio_ - halfWidth_)) - 1;
    const int64_t drop = keepFrom - base_;
    if (drop > 65536) {
        buf_.erase(buf_.begin(), buf_.begin() + static_cast<std::ptrdiff_t>(drop * ch_));
        base_ += drop;
    }
}

} // namespace w2a
