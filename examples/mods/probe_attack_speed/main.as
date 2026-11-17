// Measurement mod for Override_CombatMove (from API 45): changes the playback speed of the combat movements (sAICombatMoveStart).
// Settings in mod.ini [settings] or g3asm\settings\probe_attack_speed.ini: Faktor (default 2.0, easy to see), AlleNPCs (default no).
// The log shows the first 20 changes.
int gemeldet = 0;

bool Override_CombatMove(Entity self, const bCString &in move, uint percent) {
    const float faktor = GetModSettingFloat("Faktor", 1.0f);
    if (faktor != 1.0f && (self == GetHero() || GetModSettingBool("AlleNPCs"))) {
        const uint neu = uint(percent * faktor + 0.5f);
        SetOverrideArg(2, neu);
        if (gemeldet < 20) {
            gemeldet++;
            print(self.GetName() + " " + move + ": " + percent + " % -> " + neu + " %");
        }
    }
    return Super();
}
