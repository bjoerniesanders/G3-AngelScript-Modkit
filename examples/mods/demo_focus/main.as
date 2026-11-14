// Fokus und Fadenkreuz.
//   Pos1  meldet das anvisierte Entity (Fokus), das Entity unter dem Fadenkreuz und den Zielpunkt der Welt (Name, Abstand, Position, Weltmatrix)
//   Entf  entfernt den NPC unter dem Fadenkreuz, wenn er per SpawnNPC erzeugt wurde (Welt-NPCs bleiben)
// Protokoll: g3asm.log.

bCString Beschreibe(const bCString &in was, Entity e, Entity held) {
    if (!e.IsValid()) return was + ": nichts";
    bCVector g = e.GetBoundsMax() - e.GetBoundsMin();
    Entity eltern = e.GetParent();
    return was + " [" + e.GetCategory() + "]: '" + e.GetName() + "' Vorlage '" + e.GetTemplateName() + "' Groesse " + g.ToString() + " Eltern '" + (eltern.IsValid() ? eltern.GetName() : bCString("-")) + "' (Abstand " + held.GetDistanceTo(e) + ", NPC " + IsNPC(e) + ", erzeugter NPC " + IsDynamicSpawn(e) + ")";
}

void OnKey(int vk, bool down) {
    if (!down) return;
    Entity held = GetHero();
    if (vk == 0x24) {          // Pos1
        Entity fokus = GetFocusedEntity();
        Entity kreuz = PickCenter();
        bCVector ziel = GetPickPoint();
        bCString t1 = Beschreibe("Fokus", fokus, held), t2 = Beschreibe("Fadenkreuz", kreuz, held);
        print(t1 + " | " + t2 + " | Zielpunkt " + ziel.ToString());
        if (fokus.IsValid()) print("  Fokus-Weltmatrix Verschiebung " + fokus.GetWorldMatrix().GetTranslation().ToString() + ", Z-Achse " + fokus.GetWorldMatrix().GetZAxis().ToString());
        PrintScreen(t1 + "\n" + t2, 5, 180, 255, 180);
    } else if (vk == 0x2E) {   // Entf
        Entity kreuz = PickCenter();
        print("Entf: Fadenkreuz " + (kreuz.IsValid() ? kreuz.GetName() + " [" + kreuz.GetCategory() + "]" : bCString("nichts")));
        if (!kreuz.IsValid()) { PrintScreen("Nichts unter dem Fadenkreuz", 3, 255, 150, 150); return; }
        bool ok = DestroyNPC(kreuz);
        PrintScreen(ok ? "Entfernt" : "Nicht entfernt (kein erzeugter NPC)", 3, ok ? 180 : 255, ok ? 255 : 180, 150);
    }
}
