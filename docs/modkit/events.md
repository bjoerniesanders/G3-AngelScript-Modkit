# Ereignisse

Ein Mod definiert die Funktionen, auf die er reagieren will. Alle sind **optional**, die **Signatur muss genau stimmen**
(Parametertypen und `const bCString &in` so schreiben). Jedes Ereignis geht der Reihe nach an alle geladenen Mods.

## Engine-Ereignisse

| Funktion | Wann |
|---|---|
| `void main()` | Einmal, sobald die Engine initialisiert ist, bzw. nach jedem Neuladen des Mods (Hot-Reload) |
| `void OnFrame(float dt)` | Jeden Frame. `dt` in Sekunden. Nicht zu viel Arbeit hineinlegen (Abbruch nach 5 s) |
| `void OnKey(int vk, bool down)` | Taste gedrückt (`down = true`) oder losgelassen. Tastenwiederholung wird nicht gemeldet |
| `void OnWorldActivate()` | Eine Welt wurde geladen/aktiviert |
| `void OnWorldDeactivate()` | Die Welt wird entladen. Entities danach nicht mehr verwenden |
| `void OnShutdown()` | Vor dem Beenden des Spiels bzw. vor dem Neuladen oder Entladen des Mods |

## Spielereignisse

Diese kommen aus dem Questsystem des Spiels (`gCQuestManager_PS` in `Game.dll`). Das Spiel meldet sie bei **jedem** Vorfall, nicht nur bei aktiven Quests.
Das Originalverhalten läuft zuerst, dann deine Funktion.

| Funktion | Wann |
|---|---|
| `void OnNPCKilled(Entity victim)` | Ein NPC wurde getötet |
| `void OnNPCDefeated(Entity npc)` | Ein NPC wurde besiegt (nicht getötet, z. B. bewusstlos) |
| `void OnReceiveItem(Entity giver, Entity receiver, const bCString &in itemName, const bCString &in itemId, uint count)` | Items wechseln den Besitzer |
| `void OnEnter(Entity who, const bCString &in area)` | Jemand betritt einen Bereich (`Silden_InnerZone`, ...) |
| `void OnLeave(Entity who, const bCString &in area)` | Jemand verlässt einen Bereich |
| `void OnSpellCast(Entity caster, Entity target, const bCString &in spell)` | Ein Zauber wird gewirkt (`Spl_Fireball`, `Spl_HealOther`, ...) |
| `void OnDamage(Entity victim, Entity inflictor, Entity owner, int amount, int type)` | Ein Treffer hat Lebenspunkte gekostet (nach dem Original). `inflictor` = Waffe/Geschoss (kann leer sein), `owner` = Verursacher, siehe `Override_Damage` (ab Version 25) |
| `void OnStatusEffect(Entity npc, int effect, int enabled)` (ab Version 27) | Ein Statuseffekt hat sich geändert (`effect` = `STATUS_…`, `enabled` 1 = an, 0 = aus). Je geändertes Bit ein Aufruf, nur bei echter Änderung (das Spiel setzt Effekte auch ständig mit unverändertem Wert neu). Gilt für NPCs und den Helden, auch wenn ein Mod den Effekt selbst setzt |
| `void OnDamage(Entity victim, Entity attacker, int amount, int type)` | Alte Form ohne `owner`, weiter unterstützt; `attacker` ist dasselbe wie `inflictor` |

Hinweise aus dem Spieltest:

- **`OnReceiveItem`:** `itemName` ist der Name der Item-Vorlage (`It_Wolf_Fur`, `It_Water`, `Arrow`), `itemId` die GUID als Text. Kann der Name nicht aufgelöst werden,
  steht in `itemName` die GUID. `giver` oder `receiver` können ungültig sein (Fund, Kauf, Abgabe); dann liefert `GetName()` einen leeren Text.
  Plünderst du eine Leiche, ist der Tote `giver` und du `receiver`. Auch Zauber lösen Meldungen aus (Schriftrollen werden verbraucht: `It_Scroll_Fireball` von dir an niemand).
- **`OnEnter`/`OnLeave`:** Die Engine meldet auch interne Navigationszonen der NPCs (`Nav_Zone`, `Nav_Path`, `Nav_NavZone 1 Edge`), ungefähr 600 Mal pro Minute.
  Der Patch filtert alle `Nav_*` standardmäßig heraus. Mit `navevents=1` in der ini kommen sie wieder.
- **`OnNPCKilled`** nennt nur das Opfer, nicht den Täter. Wer getötet hat, bekommst du über `OnSpellCast` oder andere Ereignisse davor.

## Dialog- und Lade-Ereignisse

| Funktion | Wann |
|---|---|
| `void OnInfoFinished(const bCString &in info, const bCString &in owner, const bCString &in quest)` | Ein Gespräch (Info) ist zu Ende gesprochen. Siehe [Dialoge](dialogs.md) |
| `void OnQuestsLoaded()` | Die Quests des Spiels wurden neu aufgebaut (Spielstart, Laden): hier eigene Quests anlegen |
| `void OnInfosLoaded()` | Die Dialoge des Spiels wurden neu aufgebaut: hier eigene Infos anlegen. Siehe [Speichern und Laden](saving.md) |

## Beispiel

```angelscript
void OnNPCKilled(Entity opfer) {
    print("getötet: " + opfer.GetName());
    PrintScreen("Getötet: " + opfer.GetName(), 3, 255, 120, 120);
}

void OnReceiveItem(Entity von, Entity an, const bCString &in item, const bCString &in guid, uint anzahl) {
    if (an.IsValid() && an == GetHero())
        PrintScreen("+ " + item + " x" + anzahl, 3, 140, 255, 140);
}
```

## Reihenfolge und Verschachtelung

- Mods werden in Ladereihenfolge aufgerufen, siehe [Mod-Ordner](mod-package.md).
- Feuert ein Spielereignis, während ein Skript läuft (zum Beispiel weil dein Skript ein Item entfernt), wird der Aufruf verschachtelt ausgeführt.
- Mehrere Hooks können kurz nacheinander für denselben Vorgang feuern (ein Kill erzeugt `OnSpellCast`, `OnReceiveItem` und `OnNPCKilled`).
