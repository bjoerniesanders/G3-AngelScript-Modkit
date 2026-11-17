// Make and remove NPCs.
//   Numeric keypad +      searches templates with the search text in the name (list in the log)
//   Numeric keypad ,      makes the first result at the point of the crosshair (PickCenter), if there is none 2 m next to the hero, and turns it to the hero
//   Numeric keypad -      removes the last NPC that the mod made
//   Numeric keypad *      removes all NPCs that the mod made in this session
//   Numeric keypad /      removes ALL entities that SpawnNPC made with the search text in the name, also from previous sessions (after the load
//                         of a saved game). World NPCs stay (DestroyNPC refuses entities outside of the system sector)
// If the NPC looks away from the hero, set BLICK_VERSATZ to 180. Log: g3asm.log.
const bCString SUCHE = "Diego";
const float BLICK_VERSATZ = 0;
array<Entity> erzeugt;

void OnWorldDeactivate() { erzeugt.resize(0); }   // do not keep entities across a world change

void OnKey(int vk, bool down) {
    if (IsKeyDown(0x11) || IsKeyDown(0x10)) return;   // with Ctrl or Shift, the numeric keypad keys belong to other mods (probe_lightning, probe_script, probe_events, probe_time, ...)
    if (!down) return;
    if (vk == 0x6B) {
        bCString namen = FindTemplateNames(SUCHE, 30);
        print("Vorlagen mit '" + SUCHE + "': " + namen);
        PrintScreen("Vorlagen: " + namen, 6, 255, 255, 180);
    } else if (vk == 0x6E) {
        bCString namen = FindTemplateNames(SUCHE, 1);
        if (namen == "") { PrintScreen("Keine Vorlage mit '" + SUCHE + "'", 4, 255, 120, 120); return; }
        Entity held = GetHero();
        bCVector p = held.GetPosition();
        p.x += 200 + 60 * erzeugt.length();   // fallback: each next one a little more to the right
        PickCenter();                          // ray through the center of the screen, the hero is excluded
        if (HasPickPoint()) p = GetPickPoint();
        Entity npc = SpawnNPC(namen, p);
        if (npc.IsValid()) {
            bool gedreht = npc.LookAt(held, BLICK_VERSATZ);
            erzeugt.insertLast(npc);
            PrintScreen("Erzeugt: " + namen + " (gedreht " + gedreht + "), insgesamt " + erzeugt.length(), 4, 180, 255, 180);
        } else {
            PrintScreen("Erzeugen fehlgeschlagen: " + namen, 4, 255, 120, 120);
        }
    } else if (vk == 0x6D) {
        if (erzeugt.length() == 0) { PrintScreen("Nichts zu entfernen", 3, 255, 255, 180); return; }
        Entity npc = erzeugt[erzeugt.length() - 1];
        bool ok = DestroyNPC(npc);
        erzeugt.removeLast();
        PrintScreen("Entfernt: " + ok + ", uebrig " + erzeugt.length(), 3, 180, 255, 180);
    } else if (vk == 0x6F) {
        int n = CountEntities(SUCHE), weg = 0, behalten = 0;
        for (int i = n - 1; i >= 0; i--) {   // backward: after a removal, the indexes move
            Entity e = GetEntityByPart(SUCHE, i);
            if (e.IsValid() && IsDynamicSpawn(e)) { if (DestroyNPC(e)) weg++; } else behalten++;
        }
        erzeugt.resize(0);
        print("Bereinigen '" + SUCHE + "': " + n + " gefunden, " + weg + " entfernt, " + behalten + " behalten");
        PrintScreen("Bereinigt: " + weg + " entfernt, " + behalten + " behalten", 4, 180, 255, 180);
    } else if (vk == 0x6A) {
        int n = 0;
        for (uint i = 0; i < erzeugt.length(); i++) { Entity npc = erzeugt[i]; if (DestroyNPC(npc)) n++; }
        erzeugt.resize(0);
        PrintScreen("Entfernt: " + n, 3, 180, 255, 180);
    }
}
