// Test (API 44): Einstellungen je Mod - Vorgaben aus mod.ini [settings], Benutzerwerte in G3ASM_SETTINGS_DIR\einstellungen.ini
void main() {
    ResetModSetting("Zahl");   // Rest eines abgebrochenen Laufs
    check(GetModSetting("name") == "Wolf" && GetModSetting("fehlt", "vorgabe") == "vorgabe", "Einstellungen: Text aus mod.ini, Standard bei fehlendem Schluessel");
    check(GetModSettingInt("Zahl") == 16 && GetModSettingFloat("Faktor") == 2.5f && GetModSettingBool("An"), "Einstellungen: Zahl, Komma-Zahl und ja/nein aus mod.ini");
    check(GetModSettingInt("Name", -1) == -1 && HasModSetting("FAKTOR") && !HasModSetting("fehlt"), "Einstellungen: kein Zahlwert -> Standard, Schluessel ohne Gross-/Kleinschreibung");
    check(SetModSetting("Zahl", "7") && GetModSettingInt("zahl") == 7, "Einstellungen: Benutzerwert ueberdeckt die Vorgabe");
    check(!SetModSetting("a=b", "1") && !SetModSetting("", "1"), "Einstellungen: ungueltiger Schluessel abgelehnt");
    check(ResetModSetting("Zahl") && GetModSettingInt("Zahl") == 16, "Einstellungen: Zuruecksetzen liefert wieder die Vorgabe");
}
