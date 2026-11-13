// Beispiel-Mod: zeigt Spielereignisse (Kills, Items, Bereiche, Zauber) als Bildschirmtext und im Protokoll.
// Installation: Ordner nach <Gothic3>\g3asm\mods\ereignisse kopieren. Abschalten: in g3asm\mods\load_order.txt "-ereignisse" eintragen.
// Bildschirmtext nur fuer den Helden (Items, Bereiche, Zauber); alles andere nur im Protokoll.
bool IstHeld(Entity e) { return e.IsValid() && e == GetHero(); }

void OnNPCKilled(Entity opfer) {
    print("getoetet: " + opfer.GetName());
    PrintScreen("Getoetet: " + opfer.GetName(), 3, 255, 120, 120);
}
void OnNPCDefeated(Entity npc) {
    print("besiegt: " + npc.GetName());
    PrintScreen("Besiegt: " + npc.GetName(), 3, 255, 200, 100);
}
void OnReceiveItem(Entity von, Entity an, const bCString &in item, const bCString &in guid, uint anzahl) {
    print("Item " + item + " x" + anzahl + " von " + von.GetName() + " an " + an.GetName());
    if (IstHeld(an)) PrintScreen("+ " + item + " x" + anzahl, 3, 140, 255, 140);
    else if (IstHeld(von)) PrintScreen("- " + item + " x" + anzahl, 3, 255, 160, 100);
}
void OnEnter(Entity wer, const bCString &in bereich) {
    print(wer.GetName() + " betritt " + bereich);
    if (IstHeld(wer)) PrintScreen("Betreten: " + bereich, 3, 140, 180, 255);
}
void OnLeave(Entity wer, const bCString &in bereich) {
    print(wer.GetName() + " verlaesst " + bereich);
    if (IstHeld(wer)) PrintScreen("Verlassen: " + bereich, 3, 140, 180, 255);
}
void OnSpellCast(Entity wer, Entity ziel, const bCString &in zauber) {
    print(wer.GetName() + " zaubert " + zauber + " auf " + ziel.GetName());
    if (IstHeld(wer)) PrintScreen("Zauber: " + zauber, 2, 200, 140, 255);
}
