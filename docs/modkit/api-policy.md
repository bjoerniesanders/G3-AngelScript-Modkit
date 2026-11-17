# Versionsregeln der Skript-Schnittstelle

**Deutsch** | [English](../en/modkit/api-policy.md)

Diese Seite legt fest, was sich an der Skript-Schnittstelle zwischen zwei Versionen ändern darf. Sie gilt für alles, was in der
[API-Referenz](api-reference.md), bei den [Ereignissen](events.md) und bei den [Hooks](hooks.md) steht.

## Versionsnummer

- Die Skript-Schnittstelle hat **eine** fortlaufende Zahl: `GetApiVersion()` (im Code `G3ASM_SCRIPT_API_VERSION`). Jede Version, die etwas an der
  Schnittstelle ändert, erhöht die Zahl um 1.
- Ein Release heißt `v0.<API>.<Fix>`, zum Beispiel `v0.42.0-alpha.1`. Die mittlere Zahl ist die Skript-Schnittstelle. Die letzte Zahl zählt
  Fehlerbehebungen, die die Schnittstelle nicht ändern. `-alpha.N` und `-beta.N` sind Vorabversionen.
- Solange die erste Zahl 0 ist, gilt das Modkit nicht als fertig. Ein fertiges Modkit hat die Version 1.0. `-alpha.N`: Der Umfang wächst noch mit
  jeder Version. `-beta.N`: Die Skript-Schnittstelle ist eingefroren (siehe unten). Es kommen nur Fehlerbehebungen.

## Was eine neue Version darf

| Änderung | Erlaubt? |
|---|---|
| Neue Funktionen, Typen, Konstanten, Ereignisse, Overrides | Ja, jederzeit |
| Ein neuer Parameter **am Ende**, mit Standardwert | Ja |
| Das Verhalten an die Beschreibung angleichen (Fehlerbehebung) | Ja, mit Hinweis in den Änderungen |
| Eine Funktion mit Vermerk **Experimentell** ändern (Name, Parameter, Verhalten) oder entfernen | Ja, mit Hinweis in den Änderungen |
| Eine Funktion **ohne** Vermerk umbenennen, ihre Parameter ändern oder sie entfernen | Nein. Nur über **Veraltet** (siehe unten) |
| Die Signatur eines Ereignisses oder Overrides ändern | Nein. Eine **neue Form** daneben anbieten. Das Modkit ruft die alte Form weiter auf. Beispiel: `OnDamage` mit und ohne `owner` |
| Reihenfolge oder Bedeutung von Override-Argumenten (`SetOverrideArg(index, ...)`) ändern | Nein |
| Zahlenwerte von Konstanten (`QUEST_*`, `STATUS_*`, `QUALITY_*`, `SLOT_*`) ändern | Nein |

## Eingefroren ab Version 46

Mit **Version 46** ist die Skript-Schnittstelle für die Beta eingefroren. Das heißt:

- Alles, was **ohne Vermerk** in der [API-Referenz](api-reference.md), bei den [Ereignissen](events.md) und bei den [Hooks](hooks.md) steht, bleibt
  so, wie es dort beschrieben ist. Eine Mod, die nur solche Funktionen benutzt und mit Version 46 läuft, läuft auch mit jeder späteren Version. Die
  Tabelle oben gilt dafür ohne Ausnahme.
- Während der Beta kommen **keine neuen Funktionen** dazu. Es erscheinen nur Fehlerbehebungen (`v0.46.x-beta.N`). Die Zahl 46 bleibt.
- Ausgenommen sind nur die Funktionen mit Vermerk **Experimentell** (Liste unten). Sie dürfen sich noch ändern oder wegfallen. Jede Änderung steht
  in den Änderungen.
- Nicht zur Schnittstelle gehören und dürfen sich jederzeit ändern: Wortlaut und Menge der Protokollzeilen, die Mess-Schalter der ini (`scriptlog`,
  `aitrace`, `anispeedlog`, `memlog` und ähnliche) und die Mess-Mods unter `examples/mods`.
- Nach der Beta kommt Version 47. Neue Funktionen kommen dann wieder nach der Tabelle oben dazu.

## Veraltete Funktionen

1. Die Funktion bekommt in der Referenz den Vermerk **Veraltet** mit dem Ersatz. Gibt es noch keinen Ersatz, sagt die Referenz das.
2. Die Funktion bleibt aufrufbar. Beim ersten Aufruf je Mod schreibt sie eine Zeile `VERALTET: ...` ins Protokoll.
3. Entfernt wird sie frühestens **fünf Versionen** nach dem Vermerk und nie in einer Fehlerbehebung (`v0.N.x`). Ab Version 1.0 des Modkits nur noch
   mit einer neuen ersten Zahl (2.0).

| Funktion | Veraltet seit | Ersatz |
|---|---|---|
| `Entity::DoDamage` | Version 39, **entfernt mit 46** | `Entity::Kill` (Töten), `HurtHitPoints` (nur Lebenspunkte senken) |

## Experimentelle Funktionen

**Experimentell** heißt: Die Funktion läuft, aber ich habe ihre Wirkung im Spiel nicht vollständig gemessen. Wer eine solche Funktion benutzt, liest
nach jeder neuen Version die Änderungen. Eine Funktion verliert den Vermerk, sobald ich ihr Verhalten im Spiel gemessen und in der Referenz
beschrieben habe. Das ist keine Änderung der Schnittstelle.

Experimentell in Version 46: `HurtHitPoints`, `SetProperty`, `GenerateTradeGoods`, `Override_CombatMove`.

Mit Version 46 entfernt, weil sie nie funktionierten: `GetPickPosition` (Ersatz: `PickCenter` und `GetPickPoint`), `ExecuteInfo`, `DumpQuest`.

## In einer Mod auf die Version prüfen

```angelscript
void main() {
    if (GetApiVersion() < 37) {
        print("Diese Mod braucht die Skript-Schnittstelle 37 (OnTouch). Bitte G3ASM aktualisieren.");
        return;
    }
}
```

Gibt es eine Funktion in der laufenden Version nicht, ist das ein **Übersetzungsfehler**. Das Modkit lädt die Mod dann nicht, und das Protokoll
nennt die Zeile. Die Prüfung oben hilft deshalb nur bei Verhalten, das sich zwischen Versionen geändert hat. Ab Version 40 ist der Eintrag `api=37`
in der `mod.ini` besser. Dann lädt das Modkit die Mod gar nicht erst und meldet, dass das Modkit zu alt ist ([Mod-Paket](mod-package.md)).
