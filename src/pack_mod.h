// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// PackMod: folder -> Gothic 3 archive (G3V0). The result is the same as the output of G3PakDir (Union Starter), and the engine loads it as a .pak.
// Mod archives have the extensions .mod (.m00, .m01, ...) or .nod (.n00, ...): they continue the sequence of the .pak files, the engine always loads them, and they overwrite
// all normal files. No gap in the numbers (the engine stops at the first gap). .nod applies only when "Alternative Balancing" is active.
// No dependency on the script host or the game (also used as the command-line tool g3asm_packmod).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace g3asm {

struct PackOptions {
    bool comments = false;   // keep the source path as a comment for each file (G3PakDir default; it shows local paths, thus off here)
    bool deletions = true;   // "<name>.deleted" -> entry "file deleted" (removes the file from the layer below)
};

struct PackStats {
    int files = 0, dirs = 0, deleted = 0;
    uint64_t bytes = 0;   // Nutzdaten
    std::string error;    // empty = success
};

// Write a folder (recursive) into an archive. Paths in the archive are relative to the folder, with '/'. Directories end with '/'.
// The function writes to <outFile>.tmp first, then renames it. If there is an error, an existing target file does not change.
bool PackFolder(const std::string& srcDir, const std::string& outFile, const PackOptions& opt, PackStats* stats);

// Next free mod file for a volume (archive name without extension, for example "Quests"): kind 'm' -> Quests.mod, Quests.m00, ... ; kind 'n' -> Quests.nod, Quests.n00, ...
// Return value: the full path in dataDir, empty if all 101 places are used.
std::string NextGenerationFile(const std::string& dataDir, const std::string& volume, char kind);

// Put the folder as the next mod generation of <volume> into <dataDir>. Return value: the path of the new file, empty if there is an error (stats->error).
std::string PackMod(const std::string& srcDir, const std::string& dataDir, const std::string& volume, char kind, const PackOptions& opt,
                    PackStats* stats);

// Read (check and tool): the entries of an archive. Contents only for entries that are not compressed.
struct ArchiveEntry {
    std::string name;   // full path in the archive (separated by '/', directories with '/' at the end)
    bool isDir = false, deleted = false, compressed = false;
    uint32_t attrs = 0;
    uint64_t size = 0;   // size before packing
    uint64_t offset = 0, stored = 0;
    std::string comment;
};
bool ListArchive(const std::string& file, std::vector<ArchiveEntry>* out, std::string* error);
bool ReadArchiveEntry(const std::string& file, const ArchiveEntry& e, std::string* data);

}   // namespace g3asm
