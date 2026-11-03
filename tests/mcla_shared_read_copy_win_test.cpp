// KI-065 (M10-008): the F10 save copy must never block the game's writes,
// renames or deletes of a save file. Owned temporary fixture only.

#include "mcla_shared_read_copy_win.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace mcla::diagnostics;
namespace fs = std::filesystem;

unsigned checks = 0, failures = 0;
void Check(bool value, const char *label) {
  ++checks;
  if (!value) {
    ++failures;
    std::cerr << "FAIL " << label << '\n';
  }
}

std::vector<uint8_t> Bytes(size_t count, uint8_t seed) {
  std::vector<uint8_t> out(count);
  for (size_t i = 0; i < count; ++i) out[i] = static_cast<uint8_t>(seed + i * 7);
  return out;
}

void WriteBytes(const fs::path &path, const std::vector<uint8_t> &bytes) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

std::vector<uint8_t> ReadBytes(const fs::path &path) {
  std::ifstream in(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

// The share mode the SDK uses for guest file opens (core/filesystem_win.cpp).
constexpr DWORD kGameShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;

HANDLE GameOpenForWrite(const fs::path &path, DWORD disposition) {
  return CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, kGameShare, nullptr,
                     disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void TestNames() {
  Check(IsTransactionScratchName(L"SAVEGAME.rexbackup"), "package backup is scratch");
  Check(IsTransactionScratchName(L"SAVEGAME.header.rexbackup"), "header backup is scratch");
  Check(IsTransactionScratchName(L"SAVEGAME.header.rexpending"), "marker is scratch");
  Check(!IsTransactionScratchName(L"SAVEGAME"), "package is not scratch");
  Check(!IsTransactionScratchName(L"SAVEGAME.header"), "header is not scratch");
  Check(!IsTransactionScratchName(L"rexbackup"), "bare word is not scratch");
  Check(!IsTransactionScratchName(L"x.rexbackup.tmp"), "suffix must end the name");
  Check(!IsTransactionScratchName(L""), "empty name is not scratch");
}

void TestCopy(const fs::path &root) {
  const auto source = root / L"source.bin";
  const auto destination = root / L"copy.bin";
  const auto bytes = Bytes(300 * 1024 + 17, 3);  // several read chunks
  WriteBytes(source, bytes);
  auto result = CopyFileSharedRead(source, destination, 16ull * 1024 * 1024);
  Check(result.copied && result.error == ERROR_SUCCESS, "copy succeeds");
  Check(result.bytes == bytes.size(), "copy reports the byte count");
  Check(ReadBytes(destination) == bytes, "copy is byte-identical");

  const auto smaller = Bytes(10, 9);
  WriteBytes(source, smaller);
  result = CopyFileSharedRead(source, destination, 16ull * 1024 * 1024);
  Check(result.copied && ReadBytes(destination) == smaller, "copy replaces a longer destination");

  WriteBytes(source, {});
  result = CopyFileSharedRead(source, destination, 16ull * 1024 * 1024);
  Check(result.copied && result.bytes == 0 && fs::file_size(destination) == 0, "empty file copies");

  const auto big_destination = root / L"too-big.bin";
  WriteBytes(source, Bytes(4097, 1));
  result = CopyFileSharedRead(source, big_destination, 4096);
  Check(!result.copied && result.error == ERROR_FILE_TOO_LARGE, "oversized source refused");
  Check(!fs::exists(big_destination), "oversized source writes nothing");

  result = CopyFileSharedRead(root / L"missing.bin", root / L"missing-copy.bin", 4096);
  Check(!result.copied && result.error == ERROR_FILE_NOT_FOUND, "missing source reports not found");
  Check(!fs::exists(root / L"missing-copy.bin"), "missing source writes nothing");

  result = CopyFileSharedRead(source, root / L"no-such-dir" / L"copy.bin", 1u << 20);
  Check(!result.copied && result.error == ERROR_PATH_NOT_FOUND, "missing destination folder reported");
}

// While the copier holds its source handle, the game can still open the file
// for writing, truncate it, rename it and delete it.
void TestCopierNeverBlocksTheGame(const fs::path &root) {
  const auto save = root / L"SAVEGAME";
  WriteBytes(save, Bytes(4096, 5));
  {
    OwnedHandle copier(OpenSharedRead(save));
    Check(copier.valid(), "copier opens the save");
    OwnedHandle writer(GameOpenForWrite(save, OPEN_EXISTING));
    Check(writer.valid(), "game opens the save for write while the copier reads");
    OwnedHandle truncate(GameOpenForWrite(save, TRUNCATE_EXISTING));
    Check(truncate.valid(), "game truncates the save while the copier reads");
    writer.Close();
    truncate.Close();
    const auto renamed = root / L"SAVEGAME.renamed";
    Check(MoveFileExW(save.c_str(), renamed.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE,
          "game renames the save while the copier reads");
    Check(MoveFileExW(renamed.c_str(), save.c_str(), MOVEFILE_REPLACE_EXISTING) != FALSE,
          "game renames it back while the copier reads");
  }
  {
    OwnedHandle copier(OpenSharedRead(save));
    Check(DeleteFileW(save.c_str()) != FALSE, "game deletes the save while the copier reads");
  }
  Check(!fs::exists(save), "delete completes once the copier closes");

  // Controls: a reader without FILE_SHARE_WRITE blocks the game's write open;
  // a reader without FILE_SHARE_DELETE (read|write only) blocks the game's
  // rename and delete of the save.
  WriteBytes(save, Bytes(4096, 6));
  {
    OwnedHandle strict(CreateFileW(save.c_str(), GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                   OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    Check(strict.valid(), "control reader (no write sharing) opens");
    HANDLE writer = GameOpenForWrite(save, OPEN_EXISTING);
    const DWORD error = GetLastError();
    Check(writer == INVALID_HANDLE_VALUE && error == ERROR_SHARING_VIOLATION,
          "control: a reader without write sharing blocks the game's write open");
    if (writer != INVALID_HANDLE_VALUE) CloseHandle(writer);
  }
  {
    OwnedHandle no_delete(CreateFileW(save.c_str(), GENERIC_READ,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    Check(no_delete.valid(), "control reader (no delete sharing) opens");
    const auto renamed = root / L"SAVEGAME.blocked";
    const BOOL moved = MoveFileExW(save.c_str(), renamed.c_str(), MOVEFILE_REPLACE_EXISTING);
    const DWORD move_error = GetLastError();
    Check(!moved && move_error == ERROR_SHARING_VIOLATION,
          "control: a reader without delete sharing blocks the game's rename");
    if (moved) MoveFileExW(renamed.c_str(), save.c_str(), MOVEFILE_REPLACE_EXISTING);
    const BOOL deleted = DeleteFileW(save.c_str());
    const DWORD delete_error = GetLastError();
    Check(!deleted && delete_error == ERROR_SHARING_VIOLATION,
          "control: a reader without delete sharing blocks the game's delete");
  }
}

// The game holds the save open for writing (as during a save); the copier
// still reads it. std::filesystem::copy_file is recorded for information.
void TestCopyWhileGameWrites(const fs::path &root) {
  const auto save = root / L"WRITING";
  const auto bytes = Bytes(8192, 11);
  WriteBytes(save, bytes);
  OwnedHandle writer(GameOpenForWrite(save, OPEN_EXISTING));
  Check(writer.valid(), "game holds the save open for write");
  const auto result = CopyFileSharedRead(save, root / L"WRITING.copy", 1u << 20);
  Check(result.copied && ReadBytes(root / L"WRITING.copy") == bytes,
        "copier reads a save the game holds open for write");
  // Information only (platform dependent, not checked): the sharing that
  // std::filesystem::copy_file grants on its source, probed with handles the
  // game could hold. "ok" means copy_file's source open grants that sharing.
  std::error_code ec;
  fs::copy_file(save, root / L"WRITING.stdcopy", fs::copy_options::overwrite_existing, ec);
  std::cout << "info: copy_file while the game holds the save for write (FILE_SHARE_WRITE): "
            << (ec ? "failed (" + std::to_string(ec.value()) + ")" : std::string("ok")) << '\n';
  writer.Close();
  {
    OwnedHandle deleter(CreateFileW(save.c_str(), DELETE | GENERIC_READ, kGameShare, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    Check(deleter.valid(), "probe handle with delete access opens");
    ec.clear();
    fs::copy_file(save, root / L"WRITING.stdcopy2", fs::copy_options::overwrite_existing, ec);
    std::cout << "info: copy_file while a handle with delete access is open (FILE_SHARE_DELETE): "
              << (ec ? "failed (" + std::to_string(ec.value()) + ")" : std::string("ok")) << '\n';
    const auto ours = CopyFileSharedRead(save, root / L"WRITING.copy2", 1u << 20);
    Check(ours.copied, "copier reads a save while a handle with delete access is open");
  }
}
}  // namespace

int main() {
  TestNames();
  wchar_t temporary[MAX_PATH + 1]{};
  const DWORD length = GetTempPathW(MAX_PATH + 1, temporary);
  if (!length || length > MAX_PATH) {
    std::cerr << "temporary directory unavailable\n";
    return 1;
  }
  const fs::path root = fs::path(temporary) /
                        (L"mcla-shared-read-copy-" + std::to_wstring(GetCurrentProcessId()) +
                         L"-" + std::to_wstring(GetTickCount64()));
  std::error_code ec;
  if (!fs::create_directories(root, ec) || ec) {
    std::cerr << "fixture directory unavailable\n";
    return 1;
  }
  TestCopy(root);
  TestCopierNeverBlocksTheGame(root);
  TestCopyWhileGameWrites(root);
  fs::remove_all(root, ec);
  std::cout << checks << " checks, " << failures << " failures\n";
  return failures ? 1 : 0;
}
