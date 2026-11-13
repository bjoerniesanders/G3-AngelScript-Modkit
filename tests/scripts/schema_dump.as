// Schema-Dump ohne Spiel: mit der echten SharedBase.dll und (ueber G3ASM_TEST_GAMEDLL) der echten Game.dll aufrufen, dann sind auch die Klassen von
// Engine und Game registriert. Ergebnis: g3asm\property_schema.json neben g3asm.dll.
//   set G3ASM_TEST_GAMEDLL=G:\Gothic3\Game.dll
//   g3asm_test.exe G:\Gothic3\SharedBase.dll g3-angelscript-modkit\tests\scripts\schema_dump.as
void main() {
    const int n = DumpPropertySchema("property_schema.json");
    check(n > 0, "DumpPropertySchema: Klassen geschrieben (" + n + ")");
}
// Das Testprogramm prueft immer auch die Override-Kette und erwartet diese drei Funktionen (wie in selftest.as)
bool Override_Test(uint v) { return !Super(); }
bool Override_TestCancel(uint v) { return true; }
bool Override_TestArg(uint v) { SetOverrideArg(0, v * 2); return Super(); }
