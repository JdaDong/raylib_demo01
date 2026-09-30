#include "game/storage.hpp"

#include "raylib.h"

#include <algorithm>
#include <utility>

namespace rd {

Storage::Storage(std::string root) : root_(std::move(root)) {
    while (root_.size() > 1 && (root_.back() == '/' || root_.back() == '\\')) root_.pop_back();
}

bool Storage::init() {
    const char* subdirs[] = {"", "screenshots", "replays", "shaders", "exports"};
    for (const char* s : subdirs) {
        const std::string p = *s ? path(s) : root_;
        if (!DirectoryExists(p.c_str())) MakeDirectory(p.c_str());  // MakeDirectory is recursive
        if (!DirectoryExists(p.c_str())) {
            TraceLog(LOG_ERROR, "STORAGE: cannot create %s", p.c_str());
            return false;
        }
    }
    return true;
}

std::string Storage::path(const std::string& rel) const { return rel.empty() ? root_ : root_ + "/" + rel; }

bool Storage::writeTextAtomic(const std::string& rel, const std::string& text) {
    const std::string target = path(rel), tmp = target + ".tmp", bak = target + ".bak";
    if (!SaveFileText(tmp.c_str(), text.c_str())) return false;
    if (FileExists(target.c_str())) {
        // raylib 6.0: FileCopy returns 1 on success (unlike FileRename/FileRemove -> 0)
        FileCopy(target.c_str(), bak.c_str());
    }
    // rename(2) replaces the destination atomically on POSIX
    if (FileRename(tmp.c_str(), target.c_str()) != 0) {
        FileRemove(tmp.c_str());
        return false;
    }
    return true;
}

bool Storage::readText(const std::string& rel, std::string& out, bool* fromBackup) const {
    if (fromBackup) *fromBackup = false;
    const std::string target = path(rel), bak = target + ".bak";
    for (int pass = 0; pass < 2; ++pass) {
        const std::string& p = pass == 0 ? target : bak;
        if (!FileExists(p.c_str()) || GetFileLength(p.c_str()) <= 0) continue;
        char* t = LoadFileText(p.c_str());
        if (!t) continue;
        out = t;
        UnloadFileText(t);
        if (fromBackup) *fromBackup = pass == 1;
        return true;
    }
    return false;
}

bool Storage::writeBinary(const std::string& rel, const void* data, int size) {
    return SaveFileData(path(rel).c_str(), const_cast<void*>(data), size);
}

bool Storage::readBinary(const std::string& rel, std::vector<unsigned char>& out) const {
    out.clear();
    int size = 0;
    unsigned char* d = LoadFileData(path(rel).c_str(), &size);
    if (!d) return false;
    out.assign(d, d + size);
    UnloadFileData(d);
    return true;
}

bool Storage::exists(const std::string& rel) const { return FileExists(path(rel).c_str()); }

bool Storage::remove(const std::string& rel) { return FileRemove(path(rel).c_str()) == 0; }

bool Storage::copy(const std::string& relSrc, const std::string& relDst) {
    return FileCopy(path(relSrc).c_str(), path(relDst).c_str()) != 0 && exists(relDst);
}

bool Storage::moveInto(const std::string& absSrc, const std::string& relDst) {
    const std::string dst = path(relDst);
    if (!FileExists(absSrc.c_str())) return false;
    // same filesystem: atomic rename, no copy
    if (FileRename(absSrc.c_str(), dst.c_str()) == 0 && FileExists(dst.c_str())) return true;
    // cross-device fallback: FileMove (copy + delete)
    const int r = FileMove(absSrc.c_str(), dst.c_str());
    if (r == 0 && FileExists(dst.c_str())) return true;
    // raylib 6.0 bug: FileMove checks FileCopy()==0 as success, but FileCopy returns 1.
    // The copy did happen -> finish the move ourselves.
    if (FileExists(dst.c_str())) {
        if (FileExists(absSrc.c_str())) FileRemove(absSrc.c_str());
        return !FileExists(absSrc.c_str());
    }
    return FileRename(absSrc.c_str(), dst.c_str()) == 0;
}

std::vector<std::string> Storage::list(const std::string& relDir, const char* filter, bool recursive) const {
    std::vector<std::string> out;
    const std::string dir = path(relDir);
    if (!DirectoryExists(dir.c_str())) return out;
    FilePathList files = (filter || recursive) ? LoadDirectoryFilesEx(dir.c_str(), filter, recursive)
                                               : LoadDirectoryFiles(dir.c_str());
    for (unsigned int i = 0; i < files.count; ++i) out.emplace_back(files.paths[i]);
    UnloadDirectoryFiles(files);
    std::sort(out.begin(), out.end());
    return out;
}

unsigned int Storage::count(const std::string& relDir, const char* filter, bool recursive) const {
    const std::string dir = path(relDir);
    if (!DirectoryExists(dir.c_str())) return 0;
    return (filter || recursive) ? GetDirectoryFileCountEx(dir.c_str(), filter, recursive)
                                 : GetDirectoryFileCount(dir.c_str());
}

FileInfo Storage::info(const std::string& rel) const {
    FileInfo fi;
    fi.path = path(rel);
    fi.exists = FileExists(fi.path.c_str()) || DirectoryExists(fi.path.c_str());
    fi.isFile = fi.exists && IsPathFile(fi.path.c_str());
    fi.name = GetFileName(fi.path.c_str());
    fi.stem = GetFileNameWithoutExt(fi.path.c_str());
    const char* ext = GetFileExtension(fi.path.c_str());
    fi.ext = ext ? ext : "";
    fi.dir = GetDirectoryPath(fi.path.c_str());
    if (fi.isFile) {
        fi.size = GetFileLength(fi.path.c_str());
        fi.modTime = GetFileModTime(fi.path.c_str());
    }
    return fi;
}

int Storage::pruneOldest(const std::string& relDir, const char* filter, int keep) {
    const std::vector<std::string> files = list(relDir, filter, false);
    std::vector<std::pair<long, std::string>> byTime;
    for (const auto& f : files)
        if (IsPathFile(f.c_str())) byTime.emplace_back(GetFileModTime(f.c_str()), f);
    std::sort(byTime.begin(), byTime.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second > b.second;  // newest first
    });
    int removed = 0;
    for (size_t i = static_cast<size_t>(std::max(0, keep)); i < byTime.size(); ++i)
        if (FileRemove(byTime[i].second.c_str()) == 0) ++removed;
    return removed;
}

bool Storage::migrateText(const std::string& rel, const char* search, const char* replacement) {
    const std::string p = path(rel);
    if (!FileExists(p.c_str())) return false;
    if (FileTextFindIndex(p.c_str(), search) < 0) return false;
    return FileTextReplace(p.c_str(), search, replacement) > 0;
}

bool Storage::exportAsCode(const std::string& rel, const void* data, int size) {
    return ExportDataAsCode(static_cast<const unsigned char*>(data), size, path(rel).c_str());
}

bool Storage::IsSafeFileName(const std::string& name) {
    if (name.empty() || name.size() > 64) return false;
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos) return false;
    if (name == "." || name == "..") return false;
    return IsFileNameValid(name.c_str());
}

}  // namespace rd
