// Schema-Dump ohne Spiel: mit der echten SharedBase.dll und (ueber OG3_TEST_GAMEDLL) der echten Game.dll aufrufen, dann sind auch die Klassen von
// Engine und Game registriert. Ergebnis: og3\property_schema.json neben og3_patch.dll.
//   set OG3_TEST_GAMEDLL=G:\Gothic3\Game.dll
//   og3_patch_test.exe G:\Gothic3\SharedBase.dll og3-patch\tests\scripts\schema_dump.as
void main() {
    const int n = DumpPropertySchema("property_schema.json");
    check(n > 0, "DumpPropertySchema: Klassen geschrieben (" + n + ")");
}
// Das Testprogramm prueft immer auch die Override-Kette und erwartet diese drei Funktionen (wie in selftest.as)
bool Override_Test(uint v) { return !Super(); }
bool Override_TestCancel(uint v) { return true; }
bool Override_TestArg(uint v) { SetOverrideArg(0, v * 2); return Super(); }
