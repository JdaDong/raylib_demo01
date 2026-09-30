// hashing.hpp - hex digests on top of raylib ComputeCRC32/MD5/SHA1/SHA256
#pragma once
#include <string>
#include <vector>

namespace rd {

std::string Crc32Hex(const void* data, int size);
// NOTE raylib returns MD5 as 4 little-endian words; SHA1/SHA256 as big-endian words.
std::string Md5Hex(const void* data, int size);
std::string Sha1Hex(const void* data, int size);
std::string Sha256Hex(const void* data, int size);
inline std::string Sha256Hex(const std::string& s) { return Sha256Hex(s.data(), static_cast<int>(s.size())); }

// raylib CompressData/DecompressData (DEFLATE) + EncodeDataBase64/DecodeDataBase64 wrappers
std::vector<unsigned char> Compress(const void* data, int size);
bool Decompress(const std::vector<unsigned char>& in, std::vector<unsigned char>& out);
std::string Base64Encode(const void* data, int size);
bool Base64Decode(const std::string& text, std::vector<unsigned char>& out);

}  // namespace rd
