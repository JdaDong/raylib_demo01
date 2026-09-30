// storage.hpp - save-directory manager built on raylib's file system API.
// Atomic text writes (tmp + backup + rename), backup fallback on read,
// directory listing / pruning, and a workaround for the raylib 6.0 FileMove bug.
#pragma once
#include <string>
#include <vector>

namespace rd {

struct FileInfo {
    std::string path, name, stem, ext, dir;
    int size = 0;
    long modTime = 0;
    bool exists = false;
    bool isFile = false;
};

class Storage {
public:
    explicit Storage(std::string root = "saves");

    bool init();  // creates root + subdirectories
    const std::string& root() const { return root_; }
    std::string path(const std::string& rel) const;

    bool writeTextAtomic(const std::string& rel, const std::string& text);
    bool readText(const std::string& rel, std::string& out, bool* fromBackup = nullptr) const;
    bool writeBinary(const std::string& rel, const void* data, int size);
    bool readBinary(const std::string& rel, std::vector<unsigned char>& out) const;
    bool exists(const std::string& rel) const;
    bool remove(const std::string& rel);
    bool copy(const std::string& relSrc, const std::string& relDst);
    // move an absolute file into the store (works around raylib 6.0 FileMove())
    bool moveInto(const std::string& absSrc, const std::string& relDst);

    std::vector<std::string> list(const std::string& relDir, const char* filter, bool recursive = false) const;
    unsigned int count(const std::string& relDir, const char* filter = nullptr, bool recursive = false) const;
    FileInfo info(const std::string& rel) const;
    // keep the newest `keep` files matching filter, delete the rest; returns removed count
    int pruneOldest(const std::string& relDir, const char* filter, int keep);
    // in-place text migration (FileTextFindIndex + FileTextReplace); true if replaced
    bool migrateText(const std::string& rel, const char* search, const char* replacement);
    bool exportAsCode(const std::string& rel, const void* data, int size);

    static bool IsSafeFileName(const std::string& name);

private:
    std::string root_;
};

}  // namespace rd
