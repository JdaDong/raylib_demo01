// hashing: known test vectors for raylib's CRC32/MD5/SHA1/SHA256,
// DEFLATE and Base64 roundtrips.
#include "game/hashing.hpp"
#include "testing.hpp"

#include "raylib.h"

#include <cstring>

using namespace rd;

TEST(hashing, crc32_known_vector) {
    CHECK_STREQ(Crc32Hex("123456789", 9), "cbf43926");
    CHECK_STREQ(Crc32Hex("", 0), "00000000");
}

TEST(hashing, md5_known_vectors_little_endian_words) {
    CHECK_STREQ(Md5Hex("abc", 3), "900150983cd24fb0d6963f7d28e17f72");
    CHECK_STREQ(Md5Hex("", 0), "d41d8cd98f00b204e9800998ecf8427e");
}

TEST(hashing, sha1_known_vector) {
    CHECK_STREQ(Sha1Hex("abc", 3), "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(hashing, sha256_known_vector) {
    CHECK_STREQ(Sha256Hex("abc", 3), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK_STREQ(Sha256Hex(std::string("")), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(hashing, compress_roundtrip) {
    std::string text;
    for (int i = 0; i < 200; ++i) text += "neon harvest ";
    const auto packed = Compress(text.data(), static_cast<int>(text.size()));
    CHECK(!packed.empty());
    CHECK(packed.size() < text.size() / 4);  // highly repetitive
    std::vector<unsigned char> back;
    CHECK(Decompress(packed, back));
    CHECK_EQ(std::string(back.begin(), back.end()), text);
    CHECK(!Decompress({}, back));
}

TEST(hashing, base64_roundtrip_and_validation) {
    CHECK_STREQ(Base64Encode("hello", 5), "aGVsbG8=");
    CHECK_STREQ(Base64Encode("", 0), "");
    std::vector<unsigned char> out;
    CHECK(Base64Decode("aGVsbG8=", out));
    CHECK_EQ(std::string(out.begin(), out.end()), std::string("hello"));
    CHECK(!Base64Decode("a$b=", out));   // bad alphabet
    CHECK(!Base64Decode("abc", out));    // bad length
    unsigned char bin[64];
    for (int i = 0; i < 64; ++i) bin[i] = static_cast<unsigned char>(i * 37);
    CHECK(Base64Decode(Base64Encode(bin, 64), out));
    REQUIRE(out.size() == 64u);
    CHECK(std::memcmp(out.data(), bin, 64) == 0);
}

TEST(hashing, raw_raylib_encode_counts_terminator) {
    int outSize = 0;
    char* e = EncodeDataBase64(reinterpret_cast<const unsigned char*>("hi"), 2, &outSize);
    REQUIRE(e != nullptr);
    CHECK_EQ(static_cast<int>(std::strlen(e)) + 1, outSize);  // raylib 6.0 quirk
    MemFree(e);
}
