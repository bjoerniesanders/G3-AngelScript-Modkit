// Mess-Mod fuer DumpPropertySchema (ab API 43): schreibt das Schema aller Eigenschaftsklassen bei den ersten beiden Weltaktivierungen
// (Hauptmenue, dann nach dem Laden eines Spielstands) nach <Spiel>\og3\property_schema_1.json und _2.json. Zwei Dateien zeigen, ob spaeter
// geladene DLLs (Script_*.dll) weitere Klassen anmelden. Dauer ohne Spiel gemessen: etwa 0,1 s je Dump. Keine Taste.
int laeufe = 0;

void OnWorldActivate() {
    if (laeufe >= 2) return;
    laeufe++;
    const int n = DumpPropertySchema("property_schema_" + laeufe + ".json");
    print("Dump " + laeufe + ": " + n + " Klassen");
}
