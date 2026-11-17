// Measurement mod for DumpPropertySchema (from API 43): writes the schema of all property classes at the first two world activations
// (main menu, then after the load of a saved game) to <game>\g3asm\property_schema_1.json and _2.json. Two files show if DLLs that load
// later (Script_*.dll) register more classes. Duration measured without the game: approximately 0.1 s for each dump. No key.
int laeufe = 0;

void OnWorldActivate() {
    if (laeufe >= 2) return;
    laeufe++;
    const int n = DumpPropertySchema("property_schema_" + laeufe + ".json");
    print("Dump " + laeufe + ": " + n + " Klassen");
}
