// Measurement mod for the time rate (from API 36): GetTimeScale/SetTimeScale (full simulation) and GetClockFactor/SetClockFactor (game clock).
//   Shift + numeric keypad +  simulation faster (x2, up to 4)      Shift + numeric keypad -  slower (/2, down to 0.25)
//   Shift + numeric keypad *  game clock x10 on or off (the clock is in the saved game: before the save, the mod restores the old value)
// Display at the top left (OnDraw), values in the log.
float uhrAlt = -1;
int schrift = 0;

void Melden(const bCString &in was) {
    print(was + ": Zeitrate " + GetTimeScale() + ", Uhrfaktor " + GetClockFactor() + ", Spielzeit Tag " + GetGameDay() + " " + GetGameHour() + ":" + GetGameMinute());
}

void OnWorldActivate() { Melden("Weltstart"); }

void OnKey(int vk, bool down) {
    if (!down || !IsKeyDown(0x10)) return;
    if (vk == 0x6B) {
        float s = GetTimeScale() * 2;
        if (s > 4) s = 4;
        Melden("SetTimeScale(" + s + ") = " + SetTimeScale(s));
    } else if (vk == 0x6D) {
        float s = GetTimeScale() / 2;
        if (s < 0.25f) s = 0.25f;
        Melden("SetTimeScale(" + s + ") = " + SetTimeScale(s));
    } else if (vk == 0x6A) {
        if (uhrAlt < 0) {
            uhrAlt = GetClockFactor();
            Melden("SetClockFactor(" + uhrAlt * 10 + ") = " + SetClockFactor(uhrAlt * 10));
        } else {
            Melden("SetClockFactor(" + uhrAlt + ") = " + SetClockFactor(uhrAlt));
            uhrAlt = -1;
        }
    }
}

void OnBeforeSave(const bCString &in name) {
    if (uhrAlt >= 0) SetClockFactor(uhrAlt);
}
void OnAfterSave(const bCString &in name, int ok) {
    if (uhrAlt >= 0) SetClockFactor(uhrAlt * 10);
}

void OnDraw() {
    if (!GetHero().IsValid()) return;
    if (schrift == 0) schrift = CreateFont("Arial", 16, true);
    DrawText(schrift, 30, 60, "Zeitrate " + GetTimeScale() + "  Uhr x" + GetClockFactor() + "  " + GetGameHour() + ":" + GetGameMinute(), 0xFFC0E0FF);
}
