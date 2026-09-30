#include "game/hashing.hpp"

#include "raylib.h"

#include <cstdio>

namespace rd {

namespace {
unsigned char* Bytes(const void* p) { return const_cast<unsigned char*>(static_cast<const unsigned char*>(p)); }

std::string WordsBE(const unsigned int* w, int n) {
    std::string out;
    char buf[9];
    for (int i = 0; i < n; ++i) {
        std::snprintf(buf, sizeof(buf), "%08x", w[i]);
        out += buf;
    }
    return out;
}
}  // namespace

std::string Crc32Hex(const void* data, int size) {
    char buf[9];
    std::snprintf(buf, sizeof(buf), "%08x", ComputeCRC32(Bytes(data), size));
    return buf;
}

std::string Md5Hex(const void* data, int size) {
    const unsigned int* w = ComputeMD5(Bytes(data), size);
    std::string out;
    char buf[3];
    for (int i = 0; i < 4; ++i)
        for (int b = 0; b < 4; ++b) {  // little endian bytes per word
            std::snprintf(buf, sizeof(buf), "%02x", (w[i] >> (8 * b)) & 0xFFu);
            out += buf;
        }
    return out;
}

std::string Sha1Hex(const void* data, int size) { return WordsBE(ComputeSHA1(Bytes(data), size), 5); }
std::string Sha256Hex(const void* data, int size) { return WordsBE(ComputeSHA256(Bytes(data), size), 8); }

std::vector<unsigned char> Compress(const void* data, int size) {
    int outSize = 0;
    unsigned char* c = CompressData(static_cast<const unsigned char*>(data), size, &outSize);
    std::vector<unsigned char> out;
    if (c) {
        out.assign(c, c + outSize);
        MemFree(c);
    }
    return out;
}

bool Decompress(const std::vector<unsigned char>& in, std::vector<unsigned char>& out) {
    out.clear();
    if (in.empty()) return false;
    int outSize = 0;
    unsigned char* d = DecompressData(in.data(), static_cast<int>(in.size()), &outSize);
    if (!d || outSize <= 0) {
        if (d) MemFree(d);
        return false;
    }
    out.assign(d, d + outSize);
    MemFree(d);
    return true;
}

std::string Base64Encode(const void* data, int size) {
    int outSize = 0;
    char* e = EncodeDataBase64(static_cast<const unsigned char*>(data), size, &outSize);
    std::string out;
    if (e) {
        out = e;  // outSize counts the trailing '\0'
        MemFree(e);
    }
    return out;
}

bool Base64Decode(const std::string& text, std::vector<unsigned char>& out) {
    out.clear();
    for (char c : text) {
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=';
        if (!ok) return false;
    }
    if (text.empty() || text.size() % 4 != 0) return false;
    int outSize = 0;
    unsigned char* d = DecodeDataBase64(text.c_str(), &outSize);
    if (!d) return false;
    out.assign(d, d + outSize);
    MemFree(d);
    return true;
}

}  // namespace rd
