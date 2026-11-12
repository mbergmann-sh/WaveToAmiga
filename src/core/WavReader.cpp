// SPDX-License-Identifier: MIT
#include "WavReader.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace w2a {

namespace {

uint16_t rd16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
uint64_t rd64(const uint8_t* p) { return static_cast<uint64_t>(rd32(p)) | (static_cast<uint64_t>(rd32(p + 4)) << 32); }

inline int32_t clip32(double v) {
    if (v >= 2147483647.0) return INT32_MAX;
    if (v <= -2147483648.0) return INT32_MIN;
    return static_cast<int32_t>(std::lrint(v));
}

} // namespace

bool WavReader::open(const std::filesystem::path& path, std::string& error) {
    info_ = WavInfo{};
    framesRead_ = 0;

    if (!file_.openRead(path)) {
        error = "Datei kann nicht geöffnet werden";
        return false;
    }
    file_.seek(0, SEEK_END);
    const int64_t fileSize = file_.tell();
    file_.seek(0, SEEK_SET);

    uint8_t hdr[12];
    if (file_.read(hdr, 12) != 12) {
        error = "Datei ist zu kurz für eine WAV-Datei";
        return false;
    }
    const bool rf64 = std::memcmp(hdr, "RF64", 4) == 0;
    if ((!rf64 && std::memcmp(hdr, "RIFF", 4) != 0) || std::memcmp(hdr + 8, "WAVE", 4) != 0) {
        error = "Keine gültige WAV-Datei (RIFF/WAVE-Kennung fehlt)";
        return false;
    }

    bool haveFmt = false, haveData = false, haveDs64 = false;
    uint64_t ds64DataSize = 0;
    int64_t pos = 12;

    while (pos + 8 <= fileSize) {
        uint8_t ch[8];
        if (!file_.seek(pos, SEEK_SET) || file_.read(ch, 8) != 8) break;
        uint64_t size = rd32(ch + 4);
        const int64_t body = pos + 8;

        if (std::memcmp(ch, "ds64", 4) == 0 && size >= 16) {
            uint8_t b[16];
            if (file_.read(b, 16) == 16) {
                ds64DataSize = rd64(b + 8);
                haveDs64 = true;
            }
        } else if (std::memcmp(ch, "fmt ", 4) == 0 && size >= 16) {
            uint8_t b[40] = {0};
            const size_t want = static_cast<size_t>(std::min<uint64_t>(size, sizeof(b)));
            if (file_.read(b, want) != want) {
                error = "WAV-Header (fmt) ist beschädigt";
                return false;
            }
            info_.formatTag = rd16(b);
            info_.channels = rd16(b + 2);
            info_.sampleRate = rd32(b + 4);
            info_.blockAlign = rd16(b + 12);
            info_.bitsPerSample = rd16(b + 14);
            if (info_.formatTag == 0xFFFE && size >= 40)
                info_.formatTag = rd16(b + 24);  // erste 2 Byte der SubFormat-GUID
            haveFmt = true;
        } else if (std::memcmp(ch, "data", 4) == 0) {
            if (!haveFmt) {
                error = "WAV-Header ist beschädigt (data vor fmt)";
                return false;
            }
            const uint64_t remaining = static_cast<uint64_t>(std::max<int64_t>(0, fileSize - body));
            if (size == 0xFFFFFFFFull) size = haveDs64 ? ds64DataSize : remaining;
            info_.dataOffset = static_cast<uint64_t>(body);
            info_.dataBytes = std::min(size, remaining);  // gekürzte Dateien tolerieren
            haveData = true;
            break;
        }
        pos = body + static_cast<int64_t>(size) + static_cast<int64_t>(size & 1);
    }

    if (!haveFmt || !haveData) {
        error = "WAV-Datei enthält keinen gültigen fmt-/data-Block";
        return false;
    }
    if (info_.channels == 0 || info_.sampleRate == 0 || info_.blockAlign == 0) {
        error = "WAV-Header enthält ungültige Werte";
        return false;
    }
    info_.bytesPerSample = static_cast<uint16_t>(info_.blockAlign / info_.channels);

    const bool okPcm = info_.formatTag == 1 &&
                       (info_.bytesPerSample == 1 || info_.bytesPerSample == 2 ||
                        info_.bytesPerSample == 3 || info_.bytesPerSample == 4);
    const bool okFloat = info_.formatTag == 3 && (info_.bytesPerSample == 4 || info_.bytesPerSample == 8);
    if (!okPcm && !okFloat) {
        error = "Nicht unterstütztes WAV-Format (Typ " + std::to_string(info_.formatTag) + ", " +
                std::to_string(info_.bitsPerSample) + " Bit)";
        return false;
    }

    info_.totalFrames = info_.dataBytes / info_.blockAlign;
    if (info_.totalFrames == 0) {
        error = "Die WAV-Datei enthält keine Audiodaten";
        return false;
    }
    if (!file_.seek(static_cast<int64_t>(info_.dataOffset), SEEK_SET)) {
        error = "Positionierungsfehler in der WAV-Datei";
        return false;
    }
    return true;
}

size_t WavReader::readFrames(int32_t* out, size_t maxFrames) {
    const uint64_t remaining = info_.totalFrames - framesRead_;
    const size_t n = static_cast<size_t>(std::min<uint64_t>(maxFrames, remaining));
    if (n == 0) return 0;

    const size_t frameBytes = info_.blockAlign;
    raw_.resize(n * frameBytes);
    const size_t got = file_.read(raw_.data(), raw_.size()) / frameBytes;
    if (got < n) info_.totalFrames = framesRead_ + got;  // Datei war kürzer als im Header angegeben
    framesRead_ += got;

    const int outCh = outChannels();
    const size_t bps = info_.bytesPerSample;
    const uint8_t* base = raw_.data();

    auto forEachSample = [&](auto&& convert) {
        for (size_t f = 0; f < got; ++f) {
            const uint8_t* frame = base + f * frameBytes;
            for (int c = 0; c < outCh; ++c)
                out[f * outCh + c] = convert(frame + c * bps);
        }
    };

    if (info_.formatTag == 1) {
        switch (bps) {
        case 1:
            forEachSample([](const uint8_t* p) { return static_cast<int32_t>((static_cast<int>(p[0]) - 128) * 16777216); });
            break;
        case 2:
            forEachSample([](const uint8_t* p) { return static_cast<int32_t>(static_cast<int16_t>(rd16(p))) * 65536; });
            break;
        case 3:
            forEachSample([](const uint8_t* p) {
                int32_t v = static_cast<int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
                if (v & 0x800000) v -= 0x1000000;
                return v * 256;
            });
            break;
        default:  // 4 Byte
            forEachSample([](const uint8_t* p) { return static_cast<int32_t>(rd32(p)); });
            break;
        }
    } else if (bps == 4) {
        forEachSample([](const uint8_t* p) {
            float x;
            std::memcpy(&x, p, 4);
            return clip32(static_cast<double>(x) * 2147483648.0);
        });
    } else {
        forEachSample([](const uint8_t* p) {
            double x;
            std::memcpy(&x, p, 8);
            return clip32(x * 2147483648.0);
        });
    }
    return got;
}

} // namespace w2a
