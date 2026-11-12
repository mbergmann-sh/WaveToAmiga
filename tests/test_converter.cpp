// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michael Bergmann
// Selbsttest für WavReader, Resampler und die Konvertierung nach 8SVX, 8SVX-Fibonacci, AIFF, 16SV und MAUD.
// Erzeugt WAV-Dateien, konvertiert sie mit der eingebundenen libsox und prüft die IFF-/AIFF-Strukturen.
#define _USE_MATH_DEFINES
#include "core/Converter.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
static int g_failures = 0;

#define CHECK(cond, msg)                                                                      \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            std::cerr << "  FEHLER: " << msg << "  [" #cond "]  (Zeile " << __LINE__ << ")\n"; \
            ++g_failures;                                                                     \
        }                                                                                     \
    } while (0)

// ------------------------------------------------------------------------------------ WAV erzeugen
static void w16(std::ofstream& f, uint16_t v) { f.put(char(v & 0xFF)); f.put(char(v >> 8)); }
static void w32(std::ofstream& f, uint32_t v) { w16(f, v & 0xFFFF); w16(f, v >> 16); }

// format: 1 = PCM, 3 = Float; bytes = Bytes pro Sample; freq0 = Frequenz Kanal 0 (Kanal c: +110*c)
static void writeWav(const fs::path& p, int channels, int rate, int format, int bytes, double seconds,
                     double freq0 = 440.0, double amp = 0.5) {
    std::ofstream f(p, std::ios::binary);
    const uint32_t frames = uint32_t(seconds * rate);
    const uint32_t dataBytes = frames * channels * bytes;
    f.write("RIFF", 4); w32(f, 36 + dataBytes); f.write("WAVE", 4);
    f.write("fmt ", 4); w32(f, 16);
    w16(f, format); w16(f, channels); w32(f, rate);
    w32(f, rate * channels * bytes); w16(f, channels * bytes); w16(f, bytes * 8);
    f.write("data", 4); w32(f, dataBytes);
    for (uint32_t i = 0; i < frames; ++i) {
        const double t = double(i) / rate;
        for (int c = 0; c < channels; ++c) {
            const double x = amp * std::sin(2 * M_PI * (freq0 + 110.0 * c) * t);
            if (format == 3 && bytes == 4) { float v = float(x); f.write(reinterpret_cast<char*>(&v), 4); }
            else if (bytes == 1) { f.put(char(uint8_t(128 + x * 127))); }
            else if (bytes == 2) { w16(f, uint16_t(int16_t(std::lrint(x * 32767)))); }
            else if (bytes == 3) { int32_t v = int32_t(std::lrint(x * 8388607)); f.put(char(v & 0xFF)); f.put(char((v >> 8) & 0xFF)); f.put(char((v >> 16) & 0xFF)); }
            else { w32(f, uint32_t(int32_t(std::lrint(x * 2147483647.0)))); }
        }
    }
}

// ------------------------------------------------------------------------------------ IFF/AIFF lesen
struct Chunk { size_t offset = 0, size = 0; };   // offset = Beginn der Nutzdaten
struct Iff {
    std::vector<uint8_t> d;
    std::string form;
    uint32_t formSize = 0;
    std::map<std::string, Chunk> chunks;
    bool ok = false;
    uint32_t be32(size_t o) const { return (uint32_t(d[o]) << 24) | (uint32_t(d[o + 1]) << 16) | (uint32_t(d[o + 2]) << 8) | d[o + 3]; }
    uint16_t be16(size_t o) const { return uint16_t((d[o] << 8) | d[o + 1]); }
};

static Iff readIff(const fs::path& p) {
    Iff r;
    std::ifstream f(p, std::ios::binary);
    r.d.assign(std::istreambuf_iterator<char>(f), {});
    if (r.d.size() < 12 || std::memcmp(r.d.data(), "FORM", 4) != 0) return r;
    r.formSize = r.be32(4);
    r.form.assign(reinterpret_cast<const char*>(&r.d[8]), 4);
    size_t pos = 12;
    while (pos + 8 <= r.d.size()) {
        std::string id(reinterpret_cast<const char*>(&r.d[pos]), 4);
        const size_t size = r.be32(pos + 4);
        r.chunks[id] = {pos + 8, size};
        pos += 8 + size + (size & 1);
    }
    r.ok = (pos == r.d.size()) && (size_t(r.formSize) + 8 == r.d.size());   // Chunks lückenlos + FORM-Größe stimmt
    return r;
}

static double aiffRate(const Iff& a, size_t o) {   // 80-Bit-Extended
    const int exp = ((a.d[o] & 0x7F) << 8) | a.d[o + 1];
    uint64_t mant = 0;
    for (int i = 0; i < 8; ++i) mant = (mant << 8) | a.d[o + 2 + i];
    return std::ldexp(double(mant), exp - 16383 - 63);
}

// Referenz-Dekoder der Fibonacci-Delta-Kompression nach EA IFF 8SVX Appendix C (High-Nibble zuerst)
static std::vector<int> fibDecode(const uint8_t* src, size_t n) {
    static const int tbl[16] = {-34, -21, -13, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 13, 21};
    std::vector<int> out;
    int x = int8_t(src[1]);                    // src[0] = Pad-Byte, src[1] = Startwert
    for (size_t i = 2; i < n; ++i) {
        x += tbl[(src[i] >> 4) & 0xF]; out.push_back(x);
        x += tbl[src[i] & 0xF];        out.push_back(x);
    }
    return out;
}

static std::atomic<bool> g_noCancel{false};
static fs::path g_dir;

static w2a::ConvertResult run(const fs::path& wav, const char* name, const w2a::ConvertOptions& o) {
    const fs::path out = g_dir / name;
    return w2a::convertWav(wav, out, o, g_noCancel);
}

int main() {
    g_dir = fs::temp_directory_path() / "w2a_test";
    fs::remove_all(g_dir);
    fs::create_directories(g_dir);
    std::cout << "libsox " << w2a::soxVersion() << "\n";

    const fs::path wavSt = g_dir / "st16.wav", wavMo = g_dir / "mo16.wav";
    writeWav(wavSt, 2, 22050, 1, 2, 1.0);
    writeWav(wavMo, 1, 22050, 1, 2, 1.0);
    const uint32_t N = 22050;

    {   std::cout << "[1] IFF-8SVX mono (SoX-Handler 8svx)\n";
        w2a::ConvertOptions o; o.target = w2a::Target::Svx8;
        auto r = run(wavMo, "a.8svx", o); CHECK(r.ok, r.error);
        auto f = readIff(g_dir / "a.8svx");
        CHECK(f.ok && f.form == "8SVX", "FORM 8SVX, Chunk-Struktur/Größen konsistent");
        CHECK(f.chunks.count("VHDR") && f.chunks.count("BODY"), "VHDR + BODY vorhanden");
        CHECK(f.be32(f.chunks["VHDR"].offset) == N, "oneShotHiSamples = Frames");
        CHECK(f.be16(f.chunks["VHDR"].offset + 12) == 22050, "samplesPerSec");
        CHECK(f.d[f.chunks["VHDR"].offset + 15] == 0, "sCompression = 0");
        CHECK(f.chunks["BODY"].size == N, "BODY = 1 Byte/Frame");
        const uint8_t* b = &f.d[f.chunks["BODY"].offset];
        int maxerr = 0;   // Vergleich mit auf 8 Bit gerundetem Original
        for (uint32_t i = 0; i < N; ++i) {
            const double x = 0.5 * std::sin(2 * M_PI * 440.0 * i / 22050.0);
            maxerr = std::max(maxerr, std::abs(int(int8_t(b[i])) - int(std::lrint(x * 127.0 * 1.0078740157))));
        }
        CHECK(maxerr <= 1, "8-Bit-Daten stimmen mit dem Original überein (max. Abweichung " << maxerr << ")");
    }
    {   std::cout << "[2] IFF-8SVX stereo (CHAN, Kanäle blockweise)\n";
        w2a::ConvertOptions o;
        auto r = run(wavSt, "b.8svx", o); CHECK(r.ok, r.error);
        auto f = readIff(g_dir / "b.8svx");
        CHECK(f.ok && f.form == "8SVX", "Struktur konsistent");
        CHECK(f.be32(f.chunks["CHAN"].offset) == 6, "CHAN = LEFT|RIGHT");
        CHECK(f.chunks["BODY"].size == 2 * N, "BODY = 2 Kanäle");
    }
    {   std::cout << "[3] IFF-8SVX mit Fibonacci-Delta (mono, ungerade Länge, stereo)\n";
        // mono, ungerade Länge
        const fs::path w = g_dir / "odd.wav";
        writeWav(w, 1, 22050, 1, 2, 1.0 + 1.0 / 22050.0);   // 22051 Frames
        w2a::ConvertOptions o; o.fibonacci = true;
        auto r = run(w, "c.8svx", o); CHECK(r.ok, r.error);
        auto f = readIff(g_dir / "c.8svx");
        CHECK(f.ok && f.form == "8SVX", "Struktur konsistent");
        CHECK(f.d[f.chunks["VHDR"].offset + 15] == 1, "sCompression = 1 (Fibonacci-Delta)");
        const uint32_t n = 22051;
        CHECK(f.be32(f.chunks["VHDR"].offset) == n, "oneShotHiSamples = unkomprimierte Frames");
        CHECK(f.chunks["BODY"].size == 2 + (n + 1) / 2, "BODY = Pad + Startwert + 2 Codes/Byte");
        auto dec = fibDecode(&f.d[f.chunks["BODY"].offset], f.chunks["BODY"].size);
        int maxerr = 0;
        for (uint32_t i = 0; i < n; ++i) {
            const double x = 0.5 * std::sin(2 * M_PI * 440.0 * i / 22050.0);
            maxerr = std::max(maxerr, std::abs(dec[i] - int(std::lrint(x * 127.0 * 1.0078740157))));
        }
        CHECK(maxerr <= 4, "Fibonacci-Dekodierung liegt nahe am Original (max. Abweichung " << maxerr << ")");
        for (int v : dec) CHECK(v >= -128 && v <= 127, "kein Überlauf im 8-Bit-Bereich");
        // stereo
        auto r2 = run(wavSt, "c2.8svx", o); CHECK(r2.ok, r2.error);
        auto f2 = readIff(g_dir / "c2.8svx");
        CHECK(f2.ok, "Stereo-Struktur konsistent");
        CHECK(f2.chunks["BODY"].size == 2 * (2 + N / 2), "BODY = 2 komprimierte Kanalblöcke");
        auto L = fibDecode(&f2.d[f2.chunks["BODY"].offset], 2 + N / 2);
        auto R = fibDecode(&f2.d[f2.chunks["BODY"].offset + 2 + N / 2], 2 + N / 2);
        int eL = 0, eR = 0;
        for (uint32_t i = 0; i < N; ++i) {
            eL = std::max(eL, std::abs(L[i] - int(std::lrint(0.5 * std::sin(2 * M_PI * 440.0 * i / 22050.0) * 128))));
            eR = std::max(eR, std::abs(R[i] - int(std::lrint(0.5 * std::sin(2 * M_PI * 550.0 * i / 22050.0) * 128))));
        }
        CHECK(eL <= 6 && eR <= 6, "Stereo-Fibonacci L/R korrekt (Abw. " << eL << "/" << eR << ")");
    }
    {   std::cout << "[4] IFF-16SV mono und stereo (16 Bit, Big-Endian, Kanäle blockweise)\n";
        w2a::ConvertOptions o; o.target = w2a::Target::Svx16;
        auto r = run(wavSt, "d.16sv", o); CHECK(r.ok, r.error);
        auto f = readIff(g_dir / "d.16sv");
        CHECK(f.ok && f.form == "16SV", "FORM 16SV, Struktur konsistent");
        CHECK(f.chunks["BODY"].size == 4 * N, "BODY = 2 Kanäle x 2 Byte");
        CHECK(f.be32(f.chunks["CHAN"].offset) == 6, "CHAN = LEFT|RIGHT");
        int bad = 0;
        const size_t base = f.chunks["BODY"].offset;
        for (uint32_t i = 0; i < N; ++i) {
            const int16_t l = int16_t(std::lrint(0.5 * std::sin(2 * M_PI * 440.0 * i / 22050.0) * 32767));
            const int16_t rr = int16_t(std::lrint(0.5 * std::sin(2 * M_PI * 550.0 * i / 22050.0) * 32767));
            if (int16_t(f.be16(base + 2 * i)) != l) ++bad;
            if (int16_t(f.be16(base + 2 * N + 2 * i)) != rr) ++bad;
        }
        CHECK(bad == 0, "16-Bit-Daten bitgenau (L- und R-Block), Abweichungen: " << bad);
    }
    {   std::cout << "[5] AIFF: 16 Bit, 24 Bit (wie Quelle), 32 Bit aus Float, 8 Bit erzwungen\n";
        w2a::ConvertOptions o; o.target = w2a::Target::Aiff;
        auto r = run(wavSt, "e16.aiff", o); CHECK(r.ok, r.error);
        auto f = readIff(g_dir / "e16.aiff");
        CHECK(f.ok && f.form == "AIFF", "FORM AIFF, Struktur konsistent");
        auto c = f.chunks["COMM"];
        CHECK(f.be16(c.offset) == 2 && f.be32(c.offset + 2) == N && f.be16(c.offset + 6) == 16, "COMM: 2 Kanäle, Frames, 16 Bit");
        CHECK(std::fabs(aiffRate(f, c.offset + 8) - 22050.0) < 0.5, "Abtastrate 22050 (80-Bit-Float)");
        const size_t ss = f.chunks["SSND"].offset + 8;
        int bad = 0;
        for (uint32_t i = 0; i < N; ++i) {
            const int16_t l = int16_t(std::lrint(0.5 * std::sin(2 * M_PI * 440.0 * i / 22050.0) * 32767));
            if (int16_t(f.be16(ss + 4 * i)) != l) ++bad;   // interleaved
        }
        CHECK(bad == 0, "16-Bit-AIFF-Daten bitgenau, Abweichungen: " << bad);

        const fs::path w24 = g_dir / "st24.wav"; writeWav(w24, 2, 48000, 1, 3, 0.5);
        auto r24 = run(w24, "e24.aiff", o); CHECK(r24.ok, r24.error);
        auto f24 = readIff(g_dir / "e24.aiff");
        CHECK(f24.ok && f24.be16(f24.chunks["COMM"].offset + 6) == 24, "24-Bit-Quelle -> 24-Bit-AIFF");
        const fs::path wf = g_dir / "mo32f.wav"; writeWav(wf, 1, 44100, 3, 4, 0.5);
        auto rf = run(wf, "e32.aiff", o); CHECK(rf.ok, rf.error);
        auto ff = readIff(g_dir / "e32.aiff");
        CHECK(ff.ok && ff.be16(ff.chunks["COMM"].offset + 6) == 32, "Float-Quelle -> 32-Bit-AIFF");
        w2a::ConvertOptions o8 = o; o8.aiffBits = 8;
        auto r8 = run(wavSt, "e8.aiff", o8); CHECK(r8.ok, r8.error);
        auto f8 = readIff(g_dir / "e8.aiff");
        CHECK(f8.ok && f8.be16(f8.chunks["COMM"].offset + 6) == 8, "AIFF auf 8 Bit erzwungen");
    }
    {   std::cout << "[6] AIFF mit 6 Kanälen\n";
        const fs::path w = g_dir / "six.wav"; writeWav(w, 6, 44100, 1, 2, 0.2);
        w2a::ConvertOptions o; o.target = w2a::Target::Aiff;
        auto r = run(w, "six.aiff", o); CHECK(r.ok, r.error);
        auto f = readIff(g_dir / "six.aiff");
        CHECK(f.ok && f.be16(f.chunks["COMM"].offset) == 6, "6 Kanäle in COMM");
    }
    {   std::cout << "[7] IFF-MAUD: 16 Bit, 8 Bit, u-law, A-law\n";
        struct { w2a::MaudCoding c; int stored; int comp; uint32_t mdat; const char* n; } cases[] = {
            {w2a::MaudCoding::Pcm16, 16, 0, 2 * N, "m16.maud"}, {w2a::MaudCoding::Pcm8, 8, 0, N, "m8.maud"},
            {w2a::MaudCoding::ULaw, 8, 3, N, "mu.maud"},       {w2a::MaudCoding::ALaw, 8, 2, N, "ma.maud"}};
        for (auto& c : cases) {
            w2a::ConvertOptions o; o.target = w2a::Target::Maud; o.maud = c.c;
            auto r = run(wavMo, c.n, o); CHECK(r.ok, std::string(c.n) + ": " + r.error);
            auto f = readIff(g_dir / c.n);
            CHECK(f.form == "MAUD", std::string(c.n) + ": FORM MAUD");
            CHECK(f.ok, std::string(c.n) + ": Chunk-Struktur und FORM-Größe konsistent");
            if (!f.chunks.count("MHDR")) { CHECK(false, "MHDR fehlt"); continue; }
            const size_t m = f.chunks["MHDR"].offset;
            CHECK(f.be32(m) == N, std::string(c.n) + ": Samples");
            CHECK(f.be16(m + 4) == c.stored, std::string(c.n) + ": Bits gespeichert");
            CHECK(f.be32(m + 8) == 22050, std::string(c.n) + ": Abtastrate");
            CHECK(f.be16(m + 18) == c.comp, std::string(c.n) + ": Kompressionstyp");
            CHECK(f.chunks["MDAT"].size == c.mdat, std::string(c.n) + ": MDAT-Größe");
        }
    }
    {   std::cout << "[8] Resampling: 96 kHz -> 48 kHz (IFF-Grenze 65535 Hz), 44,1 kHz -> 8 kHz, Mono-Mischung\n";
        const fs::path w96 = g_dir / "hi.wav"; writeWav(w96, 1, 96000, 1, 2, 1.0, 1000.0, 0.5);
        w2a::ConvertOptions o; o.target = w2a::Target::Svx16;
        auto r = run(w96, "rs.16sv", o); CHECK(r.ok, r.error);
        CHECK(r.outRate == 48000, "automatisch auf 48000 Hz halbiert, ist " << r.outRate);
        auto f = readIff(g_dir / "rs.16sv");
        CHECK(f.ok && f.be16(f.chunks["VHDR"].offset + 12) == 48000, "VHDR-Rate 48000");
        CHECK(std::abs(int(f.be32(f.chunks["VHDR"].offset)) - 48000) <= 1, "Frames ~ 48000");
        // Frequenz/Pegel prüfen: Nulldurchgänge und Effektivwert
        const size_t base = f.chunks["BODY"].offset; const size_t n = f.chunks["BODY"].size / 2;
        int zc = 0; double ss = 0; for (size_t i = 1; i < n; ++i) {
            const int16_t a = int16_t(f.be16(base + 2 * (i - 1))), b = int16_t(f.be16(base + 2 * i));
            if ((a < 0) != (b < 0)) ++zc;
            ss += double(b) * b; }
        const double rms = std::sqrt(ss / n) / 32767.0;
        CHECK(std::abs(zc - 2000) <= 4, "1 kHz bleibt 1 kHz (Nulldurchgänge " << zc << ")");
        CHECK(std::fabs(rms - 0.5 / std::sqrt(2.0)) < 0.01, "Pegel bleibt erhalten (RMS " << rms << ")");

        const fs::path w44 = g_dir / "w44.wav"; writeWav(w44, 2, 44100, 1, 2, 1.0, 440.0);
        w2a::ConvertOptions o2; o2.target = w2a::Target::Aiff; o2.targetRate = 8000; o2.forceMono = true;
        auto r2 = run(w44, "rs.aiff", o2); CHECK(r2.ok, r2.error);
        auto f2 = readIff(g_dir / "rs.aiff");
        CHECK(f2.ok && f2.be16(f2.chunks["COMM"].offset) == 1, "Mono-Mischung");
        CHECK(std::abs(int(f2.be32(f2.chunks["COMM"].offset + 2)) - 8000) <= 1, "8000 Frames");
        CHECK(std::fabs(aiffRate(f2, f2.chunks["COMM"].offset + 8) - 8000.0) < 0.5, "Rate 8000 Hz");
    }
    {   std::cout << "[9] Fehlerfälle: ungültige Datei, fehlende Datei, Abbruch, Unicode-Name\n";
        const fs::path bad = g_dir / "bad.wav";
        { std::ofstream f(bad, std::ios::binary); f << "das ist keine wav-datei, nur text, mehr als zwoelf bytes"; }
        w2a::ConvertOptions o;
        auto r = w2a::convertWav(bad, g_dir / "bad.8svx", o, g_noCancel);
        CHECK(!r.ok && !r.error.empty() && !fs::exists(g_dir / "bad.8svx"), "ungültige WAV -> Fehler, keine Ausgabe");
        auto r2 = w2a::convertWav(g_dir / "gibts_nicht.wav", g_dir / "x.8svx", o, g_noCancel);
        CHECK(!r2.ok, "fehlende Datei -> Fehler");
        std::atomic<bool> cancel{true};
        auto r3 = w2a::convertWav(wavSt, g_dir / "cx.8svx", o, cancel);
        CHECK(!r3.ok && r3.cancelled && !fs::exists(g_dir / "cx.8svx"), "Abbruch -> keine Teildatei");
        const fs::path uin = g_dir / u8"Wal gesang äöü ♪.wav", uout = g_dir / u8"Wal gesang äöü ♪.8svx";
        writeWav(uin, 1, 11025, 1, 2, 0.3);
        auto r4 = w2a::convertWav(uin, uout, o, g_noCancel); CHECK(r4.ok, r4.error);
        CHECK(fs::exists(uout), "Unicode-Dateiname wird geschrieben");
    }

    fs::remove_all(g_dir);
    if (g_failures) { std::cerr << g_failures << " Prüfung(en) fehlgeschlagen\n"; return 1; }
    std::cout << "Alle Tests bestanden.\n";
    return 0;
}
