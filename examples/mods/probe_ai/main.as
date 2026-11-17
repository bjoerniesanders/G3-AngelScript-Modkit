// Measurement mod for AI scripts and speech output (from API 41).
// Watches some names that the plugin G3Fixes redirects (state ZS_ObserveSuspect, AI function _AI_UseInventoryItem, callbacks OnFollowPlayer and
// OnGuidePlayer), and writes the first calls for each name with type, executor, target and result to the log. It does not change anything (Super()).
// Each spoken sentence (OnAIOutput) goes to the log with ID and text (GetSpeechText) (the first 30), and shows for 4 seconds at the bottom center of the screen (simple subtitles).
array<bCString> namen = {"ZS_ObserveSuspect", "_AI_UseInventoryItem", "OnFollowPlayer", "OnGuidePlayer"};
array<int> gezaehlt = {0, 0, 0, 0};
int saetze = 0;
bCString zeile;
float rest = 0;
int schrift = 0;

void main() {
    for (uint i = 0; i < namen.length(); ++i) WatchScript(namen[i]);
}

int Index(const bCString &in name) {
    for (uint i = 0; i < namen.length(); ++i)
        if (namen[i] == name) return int(i);
    return -1;
}

bCString Art(int kind) {
    if (kind == AI_STATE) return "Zustand";
    if (kind == AI_FUNCTION) return "KI-Funktion";
    if (kind == AI_CALLBACK) return "Callback";
    return "Routine";
}

bool Override_AI(const bCString &in name, int kind, Entity self, Entity target) {
    int i = Index(name);
    if (i >= 0 && gezaehlt[i] < 5) print("Override_AI " + Art(kind) + " " + name + ": " + self.GetName() + " -> " + target.GetName());
    return Super();
}

void OnAI(const bCString &in name, int kind, Entity self, Entity target, int result) {
    int i = Index(name);
    if (i < 0) return;
    gezaehlt[i]++;
    if (gezaehlt[i] <= 5) print("OnAI " + Art(kind) + " " + name + ": " + self.GetName() + ", Ergebnis " + result + " (Aufruf " + gezaehlt[i] + ")");
}

void OnAIOutput(Entity speaker, const bCString &in id) {
    bCString text = GetSpeechText(id);
    if (saetze == 0) print("GetLocText('FO_It_1H_Sword_01') = '" + GetLocText("FO_It_1H_Sword_01") + "', GetLocText(Satz-ID) = '" + GetLocText(id) + "'");
    if (saetze < 30) print("Satz " + speaker.GetName() + " [" + id + "]: " + text);
    saetze++;
    zeile = speaker.GetName() + ": " + (text.IsEmpty() ? id : text);
    rest = 4;
}

void OnFrame(float dt) {
    if (rest > 0) rest -= dt;
}

void OnDraw() {
    if (rest <= 0) return;
    if (schrift == 0) schrift = CreateFont("Arial", 20, true);
    int w = TextWidth(schrift, zeile);
    DrawText(schrift, (GetScreenWidth() - w) / 2, GetScreenHeight() - 120, zeile, 0xFFFFFFFF);
}
