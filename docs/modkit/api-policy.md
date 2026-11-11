# Versionsregeln der Skript-Schnittstelle

Diese Seite legt fest, was sich an der Skript-Schnittstelle zwischen zwei Versionen ändern darf. Sie gilt für alles, was in der
[API-Referenz](api-reference.md), bei den [Ereignissen](events.md) und den [Hooks](hooks.md) steht.

## Versionsnummer

- Die Skript-Schnittstelle hat **eine** fortlaufende Zahl: `GetApiVersion()` (im Code `OG3_SCRIPT_API_VERSION`). Jede Version, die etwas an der
  Schnittstelle ändert, erhöht sie um 1.
- Releases heißen `v0.<API>.<Fix>`, zum Beispiel `v0.42.0-alpha.1`: die mittlere Zahl ist die Skript-Schnittstelle, die letzte zählt Fehlerbehebungen
  ohne Änderung an der Schnittstelle. `-alpha.N` und `-beta.N` sind Vorabversionen.
- Solange die erste Zahl 0 ist, ist das Modkit eine **Alpha**: Die Regeln unten gelten, aber der Umfang dessen, was als stabil gilt, ist noch klein.

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

## Veraltete Funktionen

1. Die Funktion bekommt in der Referenz den Vermerk **Veraltet** mit dem Ersatz (oder dem Hinweis, dass es noch keinen gibt).
2. Sie bleibt aufrufbar und schreibt beim ersten Aufruf je Mod eine Zeile `VERALTET: ...` ins Protokoll.
3. Entfernt wird sie frühestens **fünf Versionen** nach der Markierung und nie in einer Fehlerbehebung (`v0.N.x`). Ab Version 1.0 des Modkits
   nur noch mit einer neuen ersten Zahl (2.0).

| Funktion | veraltet seit | Ersatz |
|---|---|---|
| `Entity::DoDamage` | Version 39 | noch keiner mit fester Menge; Verletzen und Töten über die Blitz-Beschwörung (`beliars_klaue`), nur Lebenspunkte senken mit `HurtHitPoints` |

## Experimentelle Funktionen

**Experimentell** heißt: Die Funktion läuft, aber ihre Wirkung im Spiel ist nicht vollständig gemessen. Wer sie benutzt, prüft nach jeder neuen Version
die Änderungen. Eine Funktion verliert den Vermerk, sobald ihr Verhalten im Spiel gemessen und in der Referenz beschrieben ist; das ist keine Änderung der
Schnittstelle.

Stand Version 42: `HurtHitPoints`, `SetProperty`, `SetTemplateRef`, `SetEntityRef`, `CallScript`, `GenerateTradeGoods`, `SetCrosshairVisible`, `GetSpeechText`.
Im Spiel noch nicht geprüft: `SetGameTime`, `OnTransformStart`/`OnTransformEnd`.

## In einem Mod auf die Version prüfen

```angelscript
void main() {
    if (GetApiVersion() < 37) {
        print("Dieser Mod braucht die Skript-Schnittstelle 37 (OnTouch). Bitte den OG3-Patch aktualisieren.");
        return;
    }
}
```

Eine Funktion, die es in der laufenden Version nicht gibt, ist ein **Übersetzungsfehler**: Der Mod wird dann gar nicht geladen und das Protokoll nennt die
Zeile. Die Prüfung oben hilft deshalb nur bei Verhalten, das sich zwischen Versionen geändert hat. Besser ist ab Version 40 der Eintrag `api=37` in der
`mod.ini`: Dann lädt der Patch den Mod gar nicht erst und schreibt, dass er zu alt ist ([Mod-Paket](mod-package.md)).
