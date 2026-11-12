// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
#include "Converter.h"

#include "File.h"
#include "Resampler.h"
#include "WavReader.h"

extern "C" {
#include <sox.h>
}

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <system_error>
#include <vector>

namespace w2a {

namespace {

// ---- libsox: Meldungen einfangen (statt sie auf stderr auszugeben) -------------------------------------
thread_local std::string g_soxMessage;

void soxMessageHandler(unsigned level, const char* filename, const char* fmt, va_list ap) {
    if (level > 2) return;   // 1 = Fehler, 2 = Warnung
    char buf[512];
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    g_soxMessage = buf;
    (void)filename;
}

std::once_flag g_soxOnce;
bool g_soxOk = false;

struct SoxFormatDeleter {
    void operator()(sox_format_t* f) const { if (f) sox_close(f); }
};

constexpr size_t kChunkFrames = 8192;
constexpr int kMaxIffRate = 65535;   // VHDR.samplesPerSec ist ein UWORD

int clampAiffBits(int b) {
    return (b == 8 || b == 16 || b == 24 || b == 32) ? b : 0;
}

} // namespace

bool initSox() {
    std::call_once(g_soxOnce, [] {
        g_soxOk = (sox_init() == SOX_SUCCESS);
        if (g_soxOk) {
            sox_globals_t* g = sox_get_globals();
            g->verbosity = 2;
            g->output_message_handler = soxMessageHandler;
        }
    });
    return g_soxOk;
}

std::string soxVersion() {
    const char* v = sox_version();
    return v ? v : "?";
}

ConvertResult convertWav(const std::filesystem::path& in, const std::filesystem::path& out,
                         const ConvertOptions& opt, const std::atomic<bool>& cancel,
                         const ProgressFn& progress) {
    ConvertResult res;
    if (!initSox()) {
        res.error = "Die SoX-Bibliothek konnte nicht initialisiert werden";
        return res;
    }

    // ---- Quelle öffnen -------------------------------------------------------------------------------
    WavReader wav;
    if (!wav.open(in, res.error)) return res;
    const WavInfo& info = wav.info();

    const TargetInfo& ti = targetInfo(opt.target);
    int cap = ti.maxChannels;
    if ((opt.target == Target::Svx8 || opt.target == Target::Svx16) && info.channels != 4) cap = std::min(cap, 2);
    wav.setMaxChannels(cap);
    const int srcCh = wav.outChannels();
    const int outCh = opt.forceMono ? 1 : srcCh;

    // ---- Zielrate --------------------------------------------------------------------------------------
    int outRate = opt.targetRate > 0 ? opt.targetRate : static_cast<int>(info.sampleRate);
    const bool iff = (opt.target != Target::Aiff);      // 8SVX/16SV/MAUD: IFF-Header
    if (opt.target == Target::Svx8 || opt.target == Target::Svx16) {
        while (outRate > kMaxIffRate) outRate /= 2;     // UWORD-Grenze: automatisch halbieren
    }
    (void)iff;
    const bool resample = (outRate != static_cast<int>(info.sampleRate));

    // ---- Zielkodierung ---------------------------------------------------------------------------------
    const char* soxType = "";
    sox_encoding_t encoding = SOX_ENCODING_SIGN2;
    unsigned bits = 16;
    switch (opt.target) {
    case Target::Svx8:
        soxType = opt.fibonacci ? "svxfib" : "8svx";
        bits = 8;
        break;
    case Target::Svx16:
        soxType = "16sv";
        bits = 16;
        break;
    case Target::Aiff: {
        soxType = "aiff";
        int b = clampAiffBits(opt.aiffBits);
        if (b == 0) {   // wie Quelle
            const int srcBits = info.bytesPerSample * 8;
            b = srcBits <= 8 ? 8 : srcBits <= 16 ? 16 : srcBits <= 24 ? 24 : 32;
        }
        bits = static_cast<unsigned>(b);
        break;
    }
    case Target::Maud:
        soxType = "maud";
        switch (opt.maud) {
        case MaudCoding::Pcm16: encoding = SOX_ENCODING_SIGN2; bits = 16; break;
        case MaudCoding::Pcm8:  encoding = SOX_ENCODING_UNSIGNED; bits = 8; break;
        case MaudCoding::ULaw:  encoding = SOX_ENCODING_ULAW; bits = 8; break;
        case MaudCoding::ALaw:  encoding = SOX_ENCODING_ALAW; bits = 8; break;
        }
        break;
    }

    sox_signalinfo_t signal;
    signal.rate = static_cast<sox_rate_t>(outRate);
    signal.channels = static_cast<unsigned>(outCh);
    signal.precision = bits;
    signal.length = SOX_UNSPEC;
    signal.mult = nullptr;
    sox_encodinginfo_t enc;
    std::memset(&enc, 0, sizeof enc);   // sox_init_encodinginfo() setzt opposite_endian NICHT -> sonst Zufallswert!
    sox_init_encodinginfo(&enc);
    enc.encoding = encoding;
    enc.bits_per_sample = bits;

    g_soxMessage.clear();
    const std::string outPath = out.u8string();
    std::unique_ptr<sox_format_t, SoxFormatDeleter> ft(
        sox_open_write(outPath.c_str(), &signal, &enc, soxType, nullptr, nullptr));
    auto cleanup = [&]() {
        ft.reset();
        std::error_code ec;
        std::filesystem::remove(out, ec);
    };
    if (!ft) {
        res.error = "Zieldatei kann nicht erstellt werden" + (g_soxMessage.empty() ? std::string() : " (" + g_soxMessage + ")");
        return res;
    }

    // ---- Konvertierungsschleife --------------------------------------------------------------------------
    std::vector<int32_t> in32(kChunkFrames * static_cast<size_t>(srcCh));
    std::vector<int32_t> mono;                       // Mischpuffer
    std::vector<float> fin, fout;                    // Resampler-Ein-/Ausgabe
    std::vector<int32_t> pcm;                        // zu schreibende Samples
    std::unique_ptr<Resampler> rs;
    if (resample) rs = std::make_unique<Resampler>(static_cast<double>(info.sampleRate), static_cast<double>(outRate), outCh);

    const uint64_t total = info.totalFrames;
    uint64_t done = 0;
    int lastPct = -1;
    auto report = [&](int pct) {
        if (progress && pct != lastPct) {
            lastPct = pct;
            progress(pct);
        }
    };
    auto writeSamples = [&](const int32_t* p, size_t count) -> bool {
        if (count == 0) return true;
        return sox_write(ft.get(), reinterpret_cast<const sox_sample_t*>(p), count) == count;
    };
    auto toInt32 = [](float v) -> int32_t {
        const double d = static_cast<double>(v) * 2147483648.0;
        if (d >= 2147483647.0) return INT32_MAX;
        if (d <= -2147483648.0) return INT32_MIN;
        return static_cast<int32_t>(std::lrint(d));
    };
    auto emit = [&](const std::vector<float>& f) -> bool {
        pcm.resize(f.size());
        for (size_t i = 0; i < f.size(); ++i) pcm[i] = toInt32(f[i]);
        return writeSamples(pcm.data(), pcm.size());
    };

    report(0);
    while (true) {
        if (cancel.load(std::memory_order_relaxed)) {
            cleanup();
            res.cancelled = true;
            res.error = "Abgebrochen";
            return res;
        }
        const size_t n = wav.readFrames(in32.data(), kChunkFrames);
        if (n == 0) break;

        const int32_t* src = in32.data();
        if (outCh == 1 && srcCh > 1) {               // zu Mono mischen
            mono.resize(n);
            for (size_t f = 0; f < n; ++f) {
                int64_t sum = 0;
                for (int c = 0; c < srcCh; ++c) sum += in32[f * static_cast<size_t>(srcCh) + static_cast<size_t>(c)];
                mono[f] = static_cast<int32_t>(sum / srcCh);
            }
            src = mono.data();
        }

        bool ok;
        if (rs) {
            const size_t cnt = n * static_cast<size_t>(outCh);
            fin.resize(cnt);
            for (size_t i = 0; i < cnt; ++i) fin[i] = static_cast<float>(static_cast<double>(src[i]) / 2147483648.0);
            fout.clear();
            rs->process(fin.data(), n, fout);
            ok = emit(fout);
        } else {
            ok = writeSamples(src, n * static_cast<size_t>(outCh));
        }
        if (!ok) {
            cleanup();
            res.error = "Schreibfehler" + (g_soxMessage.empty() ? std::string(" (Datenträger voll oder Zugriff verweigert?)") : " (" + g_soxMessage + ")");
            return res;
        }
        done += n;
        const uint64_t denom = std::max<uint64_t>(total, done);
        report(static_cast<int>(std::min<uint64_t>(99, done * 100 / denom)));
    }
    if (rs) {
        fout.clear();
        rs->flush(fout);
        if (!emit(fout)) {
            cleanup();
            res.error = "Schreibfehler beim Abschluss der Konvertierung";
            return res;
        }
    }

    const int rc = sox_close(ft.release());
    if (rc != SOX_SUCCESS) {
        std::error_code ec;
        std::filesystem::remove(out, ec);
        res.error = "Fehler beim Abschließen der Zieldatei" + (g_soxMessage.empty() ? std::string() : " (" + g_soxMessage + ")");
        return res;
    }

    std::error_code ec;
    const auto sz = std::filesystem::file_size(out, ec);
    report(100);
    res.ok = true;
    res.outBytes = ec ? 0 : static_cast<uint64_t>(sz);
    res.outRate = outRate;
    res.outChannels = outCh;
    return res;
}

} // namespace w2a
