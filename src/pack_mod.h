// PackMod: Ordner -> Gothic-3-Archiv (G3V0). Das Ergebnis entspricht dem, was G3PakDir (Union Starter) schreibt, und wird von der Engine wie ein .pak geladen.
// Mod-Archive tragen die Endungen .mod (.m00, .m01, ...) bzw. .nod (.n00, ...): sie setzen die Reihe der .pak-Dateien fort, werden immer geladen und ueberschreiben
// alles Normale; ohne Luecke in der Nummerierung (die Engine hoert an der ersten Luecke auf). .nod gilt nur bei aktivem "Alternativen Balancing".
// Keine Abhaengigkeit von Skript-Host oder Spiel (auch als Kommandozeilenwerkzeug og3_packmod).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace og3p {

struct PackOptions {
    bool comments = false;   // Quellpfad als Kommentar je Datei speichern (G3PakDir-Standard; verraet lokale Pfade, deshalb hier aus)
    bool deletions = true;   // "<name>.deleted" -> Eintrag "Datei geloescht" (entfernt die Datei aus der darunterliegenden Schicht)
};

struct PackStats {
    int files = 0, dirs = 0, deleted = 0;
    uint64_t bytes = 0;   // Nutzdaten
    std::string error;    // leer = Erfolg
};

// Ordner (rekursiv) in ein Archiv schreiben. Pfade im Archiv sind relativ zum Ordner, mit '/', Verzeichnisse enden auf '/'.
// Geschrieben wird zuerst in <outFile>.tmp, dann umbenannt; bei Fehler bleibt eine vorhandene Zieldatei unveraendert.
bool PackFolder(const std::string& srcDir, const std::string& outFile, const PackOptions& opt, PackStats* stats);

// Naechste freie Mod-Datei fuer ein Volume (Archivname ohne Endung, z. B. "Quests"): kind 'm' -> Quests.mod, Quests.m00, ... ; kind 'n' -> Quests.nod, Quests.n00, ...
// Rueckgabe: voller Pfad in dataDir, leer wenn alle 101 Plaetze belegt sind.
std::string NextGenerationFile(const std::string& dataDir, const std::string& volume, char kind);

// Ordner als naechste Mod-Generation von <volume> in <dataDir> ablegen. Rueckgabe: Pfad der neuen Datei, leer bei Fehler (stats->error).
std::string PackMod(const std::string& srcDir, const std::string& dataDir, const std::string& volume, char kind, const PackOptions& opt,
                    PackStats* stats);

// Lesen (Pruefung/Werkzeug): Eintraege eines Archivs; Inhalt nur fuer unkomprimierte Eintraege.
struct ArchiveEntry {
    std::string name;   // voller Pfad im Archiv ('/' getrennt, Verzeichnisse mit '/' am Ende)
    bool isDir = false, deleted = false, compressed = false;
    uint32_t attrs = 0;
    uint64_t size = 0;   // ungepackte Groesse
    uint64_t offset = 0, stored = 0;
    std::string comment;
};
bool ListArchive(const std::string& file, std::vector<ArchiveEntry>* out, std::string* error);
bool ReadArchiveEntry(const std::string& file, const ArchiveEntry& e, std::string* data);

}   // namespace og3p
