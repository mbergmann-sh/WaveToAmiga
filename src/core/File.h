// SPDX-License-Identifier: MIT
// Kleiner RAII-Wrapper um FILE* mit Unicode-Pfaden (Windows) und 64-Bit-Seek.
#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace w2a {

class File {
public:
    File() = default;
    ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;

    bool openRead(const std::filesystem::path& p)  { return open(p, false); }
    bool openWrite(const std::filesystem::path& p) { return open(p, true); }

    bool isOpen() const { return f_ != nullptr; }

    // Gibt false zurück, wenn beim Schließen/Flushen ein Fehler auftrat.
    bool close() {
        if (!f_) return true;
        const int rc = std::fclose(f_);
        f_ = nullptr;
        return rc == 0;
    }

    size_t read(void* buf, size_t bytes)        { return f_ ? std::fread(buf, 1, bytes, f_) : 0; }
    size_t write(const void* buf, size_t bytes) { return f_ ? std::fwrite(buf, 1, bytes, f_) : 0; }

    bool seek(int64_t offset, int whence) {
        if (!f_) return false;
#ifdef _WIN32
        return _fseeki64(f_, offset, whence) == 0;
#else
        return fseeko(f_, static_cast<off_t>(offset), whence) == 0;
#endif
    }

    int64_t tell() const {
        if (!f_) return -1;
#ifdef _WIN32
        return _ftelli64(f_);
#else
        return static_cast<int64_t>(ftello(f_));
#endif
    }

private:
    bool open(const std::filesystem::path& p, bool write) {
        close();
#ifdef _WIN32
        f_ = _wfopen(p.c_str(), write ? L"wb" : L"rb");
#else
        f_ = std::fopen(p.c_str(), write ? "wb" : "rb");
#endif
        return f_ != nullptr;
    }

    std::FILE* f_ = nullptr;
};

} // namespace w2a
