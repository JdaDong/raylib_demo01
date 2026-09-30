// storage: raylib file-system API through the Storage wrapper, plus
// custom file IO callbacks (virtual FS) and raylib 6.0 quirks.
#include "game/storage.hpp"
#include "testing.hpp"

#include "raylib.h"

#include <cstring>
#include <map>
#include <string>

using namespace rd;

namespace {
std::string FreshRoot(const char* name) {
    const std::string root = std::string(RD_TEST_TMP) + "/" + name;
    // clean previous run: remove known files only (never recursive deletes)
    if (DirectoryExists(root.c_str())) {
        FilePathList files = LoadDirectoryFilesEx(root.c_str(), nullptr, true);
        for (unsigned int i = 0; i < files.count; ++i)
            if (IsPathFile(files.paths[i])) FileRemove(files.paths[i]);
        UnloadDirectoryFiles(files);
    }
    return root;
}
}  // namespace

TEST(storage, init_creates_subdirs) {
    Storage s(FreshRoot("st_init") + "/");
    CHECK(s.init());
    CHECK(DirectoryExists(s.path("screenshots").c_str()));
    CHECK(DirectoryExists(s.path("replays").c_str()));
    CHECK(s.root().back() != '/');  // trailing slash normalized
}

TEST(storage, atomic_write_keeps_backup) {
    Storage s(FreshRoot("st_atomic"));
    REQUIRE(s.init());
    CHECK(s.writeTextAtomic("settings.ini", "v1"));
    CHECK(!s.exists("settings.ini.bak"));
    CHECK(s.writeTextAtomic("settings.ini", "v2"));
    CHECK(s.exists("settings.ini.bak"));
    CHECK(!s.exists("settings.ini.tmp"));
    std::string text;
    bool fromBak = true;
    CHECK(s.readText("settings.ini", text, &fromBak));
    CHECK_STREQ(text, "v2");
    CHECK(!fromBak);
    // corrupt main file (empty) -> read falls back to backup
    SaveFileText(s.path("settings.ini").c_str(), "");
    CHECK(s.readText("settings.ini", text, &fromBak));
    CHECK_STREQ(text, "v1");
    CHECK(fromBak);
}

TEST(storage, read_missing_fails) {
    Storage s(FreshRoot("st_missing"));
    REQUIRE(s.init());
    std::string t;
    CHECK(!s.readText("nope.txt", t));
    std::vector<unsigned char> b;
    CHECK(!s.readBinary("nope.bin", b));
}

TEST(storage, binary_roundtrip) {
    Storage s(FreshRoot("st_bin"));
    REQUIRE(s.init());
    unsigned char data[256];
    for (int i = 0; i < 256; ++i) data[i] = static_cast<unsigned char>(i);
    CHECK(s.writeBinary("blob.bin", data, 256));
    std::vector<unsigned char> back;
    CHECK(s.readBinary("blob.bin", back));
    REQUIRE(back.size() == 256u);
    CHECK(std::memcmp(back.data(), data, 256) == 0);
    const FileInfo fi = s.info("blob.bin");
    CHECK(fi.exists && fi.isFile);
    CHECK_EQ(fi.size, 256);
    CHECK_STREQ(fi.name, "blob.bin");
    CHECK_STREQ(fi.stem, "blob");
    CHECK_STREQ(fi.ext, ".bin");
    CHECK(fi.modTime > 0);
    CHECK(s.remove("blob.bin"));
    CHECK(!s.exists("blob.bin"));
    CHECK(!s.remove("blob.bin"));  // FileRemove returns -1 on missing
}

TEST(storage, move_into_works_around_filemove_bug) {
    Storage s(FreshRoot("st_move"));
    REQUIRE(s.init());
    const std::string src = std::string(RD_TEST_TMP) + "/move_src.txt";
    SaveFileText(src.c_str(), "payload");
    CHECK(s.moveInto(src, "screenshots/moved.txt"));
    CHECK(!FileExists(src.c_str()));
    CHECK(s.exists("screenshots/moved.txt"));
    CHECK(!s.moveInto(src, "screenshots/x.txt"));  // source gone
}

TEST(storage, raw_filemove_bug_is_pinned) {
    // raylib 6.0: FileMove() copies, then treats FileCopy()'s success value (1) as
    // failure -> returns -1 and leaves the source behind. Storage::moveInto hides this.
    const std::string src = std::string(RD_TEST_TMP) + "/fm_src.txt";
    const std::string dst = std::string(RD_TEST_TMP) + "/fm_dst.txt";
    FileRemove(dst.c_str());
    SaveFileText(src.c_str(), "x");
    SetTraceLogLevel(LOG_NONE);  // silence the expected warning
    const int r = FileMove(src.c_str(), dst.c_str());
    SetTraceLogLevel(LOG_ERROR);
    CHECK_EQ(r, -1);
    CHECK(FileExists(dst.c_str()));  // the copy did happen
    CHECK(FileExists(src.c_str()));  // but the source was not removed
    CHECK_EQ(FileCopy(src.c_str(), dst.c_str()), 1);   // 1 == success for FileCopy
    CHECK_EQ(FileRename(dst.c_str(), src.c_str()), 0); // 0 == success for FileRename
    CHECK_EQ(FileRemove(src.c_str()), 0);
}

TEST(storage, list_count_and_filter) {
    Storage s(FreshRoot("st_list"));
    REQUIRE(s.init());
    s.writeTextAtomic("exports/a.txt", "a");
    s.writeTextAtomic("exports/b.txt", "b");
    s.writeBinary("exports/c.bin", "c", 1);
    CHECK_EQ(s.list("exports", ".txt").size(), 2u);
    CHECK_EQ(s.list("exports", ".txt;.bin").size(), 3u);
    CHECK_EQ(s.count("exports", ".txt"), 2u);
    CHECK(s.count("exports") >= 3u);
    CHECK(s.list("does_not_exist", nullptr).empty());
    CHECK_EQ(s.count("does_not_exist"), 0u);
    CHECK(s.count("", ".txt", true) >= 2u);  // recursive from root
}

TEST(storage, prune_keeps_newest) {
    Storage s(FreshRoot("st_prune"));
    REQUIRE(s.init());
    for (int i = 0; i < 6; ++i) s.writeBinary("screenshots/shot_" + std::to_string(i) + ".png", "x", 1);
    const int removed = s.pruneOldest("screenshots", ".png", 2);
    CHECK_EQ(removed, 4);
    CHECK_EQ(s.count("screenshots", ".png"), 2u);
}

TEST(storage, migrate_text_in_place) {
    Storage s(FreshRoot("st_migrate"));
    REQUIRE(s.init());
    s.writeTextAtomic("old.ini", "version = 1\nvolume = 0.5\n");
    CHECK(s.migrateText("old.ini", "volume =", "master_volume ="));
    std::string t;
    s.readText("old.ini", t);
    CHECK(t.find("master_volume = 0.5") != std::string::npos);
    CHECK(!s.migrateText("old.ini", "not-there", "x"));
    CHECK(!s.migrateText("missing.ini", "a", "b"));
}

TEST(storage, export_as_code) {
    Storage s(FreshRoot("st_code"));
    REQUIRE(s.init());
    const unsigned char data[4] = {1, 2, 3, 4};
    CHECK(s.exportAsCode("exports/blob.h", data, 4));
    std::string t;
    CHECK(s.readText("exports/blob.h", t));
    CHECK(t.find("0x") != std::string::npos);
}

TEST(storage, copy) {
    Storage s(FreshRoot("st_copy"));
    REQUIRE(s.init());
    s.writeTextAtomic("a.txt", "hello");
    CHECK(s.copy("a.txt", "exports/a_copy.txt"));
    std::string t;
    CHECK(s.readText("exports/a_copy.txt", t));
    CHECK_STREQ(t, "hello");
    CHECK(!s.copy("missing.txt", "exports/x.txt"));
}

TEST(storage, safe_file_names) {
    CHECK(Storage::IsSafeFileName("replay_01.rae"));
    CHECK(!Storage::IsSafeFileName(""));
    CHECK(!Storage::IsSafeFileName("../etc/passwd"));
    CHECK(!Storage::IsSafeFileName("a/b"));
    CHECK(!Storage::IsSafeFileName(".."));
    CHECK(!Storage::IsSafeFileName("a:b"));
    CHECK(!Storage::IsSafeFileName(std::string(100, 'x')));
}

TEST(storage, path_helpers_raylib) {
    CHECK_STREQ(GetFileName("/a/b/c.tar.gz"), "c.tar.gz");
    CHECK_STREQ(GetFileNameWithoutExt("/a/b/c.png"), "c");
    CHECK_STREQ(GetFileExtension("x.PNG"), ".PNG");
    CHECK(IsFileExtension("x.PNG", ".png"));  // case-insensitive
    CHECK(IsFileExtension("x.qoi", ".png;.qoi"));
    CHECK_STREQ(GetDirectoryPath("/a/b/c.txt"), "/a/b");
    CHECK_STREQ(GetPrevDirectoryPath("/a/b/c"), "/a/b");
    CHECK(std::strlen(GetWorkingDirectory()) > 0);
    CHECK(std::strlen(GetApplicationDirectory()) > 0);
}

TEST(storage, change_directory_roundtrip) {
    const std::string before = GetWorkingDirectory();
    CHECK(ChangeDirectory(RD_TEST_TMP));
    CHECK(std::string(GetWorkingDirectory()).find("test_tmp") != std::string::npos);
    CHECK(ChangeDirectory(before.c_str()));
    CHECK_STREQ(GetWorkingDirectory(), before);
}

// ---- custom IO callbacks: a tiny in-memory file system ---------------------
namespace {
std::map<std::string, std::string>& Vfs() {
    static std::map<std::string, std::string> m;
    return m;
}
char* VfsLoadText(const char* name) {
    auto it = Vfs().find(name);
    if (it == Vfs().end()) return nullptr;
    char* out = static_cast<char*>(MemAlloc(static_cast<unsigned int>(it->second.size() + 1)));
    std::memcpy(out, it->second.c_str(), it->second.size() + 1);
    return out;  // released by UnloadFileText -> MemFree
}
bool VfsSaveText(const char* name, const char* text) {
    Vfs()[name] = text;
    return true;
}
unsigned char* VfsLoadData(const char* name, int* size) {
    auto it = Vfs().find(name);
    if (it == Vfs().end()) { *size = 0; return nullptr; }
    *size = static_cast<int>(it->second.size());
    auto* out = static_cast<unsigned char*>(MemAlloc(static_cast<unsigned int>(*size)));
    std::memcpy(out, it->second.data(), static_cast<size_t>(*size));
    return out;
}
bool VfsSaveData(const char* name, void* data, int size) {
    Vfs()[name] = std::string(static_cast<const char*>(data), static_cast<size_t>(size));
    return true;
}
}  // namespace

TEST(storage, custom_file_callbacks_virtual_fs) {
    SetSaveFileTextCallback(VfsSaveText);
    SetLoadFileTextCallback(VfsLoadText);
    SetSaveFileDataCallback(VfsSaveData);
    SetLoadFileDataCallback(VfsLoadData);

    CHECK(SaveFileText("virtual://cfg.ini", "hello vfs"));
    char* t = LoadFileText("virtual://cfg.ini");
    REQUIRE(t != nullptr);
    CHECK_STREQ(t, "hello vfs");
    UnloadFileText(t);
    CHECK(!FileExists("virtual://cfg.ini"));  // never touched the disk

    CHECK(SaveFileData("virtual://b.bin", const_cast<char*>("\x01\x02\x03"), 3));
    int size = 0;
    unsigned char* d = LoadFileData("virtual://b.bin", &size);
    REQUIRE(d != nullptr);
    CHECK_EQ(size, 3);
    CHECK_EQ(static_cast<int>(d[2]), 3);
    UnloadFileData(d);

    SetSaveFileTextCallback(nullptr);
    SetLoadFileTextCallback(nullptr);
    SetSaveFileDataCallback(nullptr);
    SetLoadFileDataCallback(nullptr);
    CHECK(LoadFileText("virtual://cfg.ini") == nullptr);  // back to the real FS
}
