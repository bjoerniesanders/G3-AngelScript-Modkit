# Versionsregeln der Skript-Schnittstelle

Diese Seite legt fest, was sich an der Skript-Schnittstelle zwischen zwei Versionen ändern darf. Sie gilt für alles, was in der
[API-Referenz](api-reference.md), bei den [Ereignissen](events.md) und den [Hooks](hooks.md) steht.

## Versionsnummer

- Die Skript-Schnittstelle hat **eine** fortlaufende Zahl: `GetApiVersion()` (im Code `G3ASM_SCRIPT_API_VERSION`). Jede Version, die etwas an der
  Schnittstelle ändert, erhöht sie um 1.
- Releases heißen `v0.<API>.<Fix>`, zum Beispiel `v0.42.0-alpha.1`: die mittlere Zahl ist die Skript-Schnittstelle, die letzte zählt Fehlerbehebungen
  ohne Änderung an der Schnittstelle. `-alpha.N` und `-beta.N` sind Vorabversionen.
- Solange die erste Zahl 0 ist, gilt das Modkit noch nicht als fertig (das wäre 1.0). `-alpha.N`: der Umfang wächst noch mit jeder Version.
  `-beta.N`: die Skript-Schnittstelle ist eingefroren (siehe unten), es kommen nur Fehlerbehebungen.

## Was eine neue Version darf

| Änderung | erlaubt? |
|---|---|
| Neue Funktionen, Typen, Konstanten, Ereignisse, Overrides | ja, jederzeit |
| Neuer Parameter **am Ende** mit Standardwert | ja |
| Verhalten an die Beschreibung angleichen (Fehlerbehebung) | ja, mit Hinweis in den Änderungen |
| Eine Funktion mit Vermerk **Experimentell** ändern (Name, Parameter, Verhalten) oder entfernen | ja, mit Hinweis in den Änderungen |
| Eine Funktion **ohne** Vermerk umbenennen, Parameter ändern oder entfernen | nein, nur über **Veraltet** (siehe unten) |
| Die Signatur eines Ereignisses oder Overrides ändern | nein; stattdessen eine **neue Form** daneben anbieten. Die alte Form wird weiter aufgerufen (Beispiel: `OnDamage` mit und ohne `owner`) |
| Reihenfolge oder Bedeutung von Override-Argumenten (`SetOverrideArg(index, ...)`) ändern | nein |
| Zahlenwerte von Konstanten (`QUEST_*`, `STATUS_*`, `QUALITY_*`, `SLOT_*`) ändern | nein |

## Eingefroren ab Version 46

Mit **Version 46** ist die Skript-Schnittstelle für die Beta eingefroren (2026-11-13). Das heißt:

- Alles, was in der [API-Referenz](api-reference.md), bei den [Ereignissen](events.md) und den [Hooks](hooks.md) **ohne Vermerk** steht, bleibt so, wie es
  dort beschrieben ist. Eine Mod, die nur solche Funktionen benutzt und mit Version 46 läuft, läuft auch mit jeder späteren Version; die Tabelle oben
  gilt dafür ohne Ausnahme.
- Während der Beta kommen **keine neuen Funktionen** dazu. Es erscheinen nur Fehlerbehebungen (`v0.46.x-beta.N`); die Zahl 46 bleibt.
- Ausgenommen sind allein die Funktionen mit Vermerk **Experimentell** (Liste unten). Sie dürfen sich noch ändern oder wegfallen, jeweils mit Hinweis in
  den Änderungen.
- Nicht zur Schnittstelle gehören und dürfen sich jederzeit ändern: Wortlaut und Menge der Protokollzeilen, die Mess-Schalter der ini (`scriptlog`,
  `aitrace`, `anispeedlog`, `memlog` und ähnliche) und die Mess-Mods unter `examples/mods`.
- Nach der Beta geht es mit Version 47 weiter; neue Funktionen kommen dann wieder nach der Tabelle oben dazu.

Die Umbenennung des Projekts (vorher OG3-Patch, jetzt G3 AngelScript Modkit) hat an der Skript-Schnittstelle nichts geändert: Dateinamen, Ordner und
ini-Abschnitt heißen anders, alle Funktionen, Ereignisse und Konstanten sind gleich geblieben. Geändert haben sich nur die automatisch vergebenen
Text-IDs (`G3ASM_INFO_...`, `G3ASM_QTOPIC_...`, `G3ASM_QDESC_...`, vorher mit `OG3_`); Mods legen sie nicht selbst an und brauchen sie normalerweise nicht.

## Veraltete Funktionen

1. Die Funktion bekommt in der Referenz den Vermerk **Veraltet** mit dem Ersatz (oder dem Hinweis, dass es noch keinen gibt).
2. Sie bleibt aufrufbar und schreibt beim ersten Aufruf je Mod eine Zeile `VERALTET: ...` ins Protokoll.
3. Entfernt wird sie frühestens **fünf Versionen** nach der Markierung und nie in einer Fehlerbehebung (`v0.N.x`). Ab Version 1.0 des Modkits
   nur noch mit einer neuen ersten Zahl (2.0).

| Funktion | veraltet seit | Ersatz |
|---|---|---|
| `Entity::DoDamage` | Version 39, **entfernt mit 46** | `Entity::Kill` (Töten), `HurtHitPoints` (nur Lebenspunkte senken) |

## Experimentelle Funktionen

**Experimentell** heißt: Die Funktion läuft, aber ihre Wirkung im Spiel ist nicht vollständig gemessen. Wer sie benutzt, prüft nach jeder neuen Version
die Änderungen. Eine Funktion verliert den Vermerk, sobald ihr Verhalten im Spiel gemessen und in der Referenz beschrieben ist; das ist keine Änderung der
Schnittstelle.

Stand Version 46: experimentell `HurtHitPoints`, `SetProperty`, `GenerateTradeGoods`, `Override_CombatMove`.
Im Spiel bestätigt mit 46 (2026-11-11): `Entity::Kill`, `SetCrosshairVisible`, `SetGameTime`, `LoadQuestFile`, `OnTransformStart`/`OnTransformEnd`.
Ohne Vermerk seit 46: `SetTemplateRef`, `SetEntityRef`, `CallScript`, `GetSpeechText` (Verhalten gemessen und beschrieben).
Entfernt mit 46 (nie funktionsfähig, Alpha): `GetPickPosition` (Ersatz `PickCenter`/`GetPickPoint`), `ExecuteInfo`, `DumpQuest`.

## In einer Mod auf die Version prüfen

```angelscript
void main() {
    if (GetApiVersion() < 37) {
        print("Diese Mod braucht die Skript-Schnittstelle 37 (OnTouch). Bitte G3ASM aktualisieren.");
        return;
    }
}
```

Eine Funktion, die es in der laufenden Version nicht gibt, ist ein **Übersetzungsfehler**: Die Mod wird dann gar nicht geladen und das Protokoll nennt die
Zeile. Die Prüfung oben hilft deshalb nur bei Verhalten, das sich zwischen Versionen geändert hat. Besser ist ab Version 40 der Eintrag `api=37` in der
`mod.ini`: Dann lädt das Modkit die Mod gar nicht erst und meldet, dass das Modkit zu alt ist ([Mod-Paket](mod-package.md)).
