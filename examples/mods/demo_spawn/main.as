// NPCs erzeugen und entfernen.
//   Numblock +      sucht Vorlagen mit dem Suchtext im Namen (Liste im Protokoll)
//   Numblock ,      erzeugt den ersten Treffer dort, wo das Fadenkreuz hinzeigt (PickCenter), sonst 2 m neben dem Helden, und dreht ihn zum Helden
//   Numblock -      entfernt den zuletzt erzeugten NPC
//   Numblock *      entfernt alle in dieser Sitzung erzeugten NPCs
//   Numblock /      entfernt ALLE per SpawnNPC erzeugten Entities mit dem Suchtext im Namen, auch solche aus frueheren Sitzungen (nach dem Laden
//                   eines Spielstands); Welt-NPCs bleiben (DestroyNPC lehnt Entities ausserhalb des Systemsektors ab)
// Schaut der NPC vom Helden weg, BLICK_VERSATZ auf 180 setzen. Protokoll: g3asm.log.
const bCString SUCHE = "Diego";
const float BLICK_VERSATZ = 0;
array<Entity> erzeugt;

void OnWorldDeactivate() { erzeugt.resize(0); }   // Entities nicht ueber Weltwechsel aufbewahren

void OnKey(int vk, bool down) {
    if (IsKeyDown(0x11) || IsKeyDown(0x10)) return;   // mit Strg oder Umschalt gehoeren die Ziffernblock-Tasten anderen Mods (probe_lightning, probe_script, probe_events, probe_time, ...)
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
        p.x += 200 + 60 * erzeugt.length();   // Rueckfall: jeder weitere ein Stueck weiter rechts
        PickCenter();                          // Strahl durch die Bildmitte, der Held wird ausgeschlossen
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
        for (int i = n - 1; i >= 0; i--) {   // rueckwaerts: nach dem Entfernen rutschen die Indizes
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
