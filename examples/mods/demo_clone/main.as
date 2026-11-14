// Template-Klonen mit fester ID: legt die Vorlage "G3ASM_Klonfell" (Kopie von It_Wolf_Fur) beim Start und bei jedem Laden neu an. Ihre ID ist ein
// Hash des Namens und damit in jedem Spielstart gleich: im Spielstand gespeicherte Klon-Items finden die Vorlage nach einem Neustart wieder.
// Numblock 0: ein Stueck ins Inventar des Helden. Protokoll: g3asm.log.
bool Sicherstellen(const bCString &in wo) {
    int rc = CloneTemplate("It_Wolf_Fur", "G3ASM_Klonfell");   // 0 angelegt, 1 war schon da, <0 Fehler (z. B. noch kein Spiel)
    RegisterText("FO_G3ASM_Klonfell", "Geklontes Wolfsfell");   // Name/Beschreibung: Stringtabelle FO_<Vorlage>, ITEMDESC_<Vorlage>
    RegisterText("ITEMDESC_G3ASM_Klonfell", "Dieses Fell wurde zur Laufzeit aus einer Vorlage geklont.");
    print("Klonvorlage (" + wo + "): rc=" + rc);
    return rc >= 0;
}

void main() { Sicherstellen("main"); }
void OnQuestsLoaded() { Sicherstellen("OnQuestsLoaded"); }   // vor dem Einlesen des Spielstands
void OnWorldActivate() { Sicherstellen("OnWorldActivate"); }

void OnKey(int vk, bool down) {
    if (!down || vk != 0x60) return;
    Sicherstellen("Taste");
    Entity held = GetHero();
    int add = held.AddItems("G3ASM_Klonfell", 1);
    print("Klon AddItems=" + add + ", Anzahl G3ASM_Klonfell=" + held.GetItemCount("G3ASM_Klonfell"));
    PrintScreen("Klonfell: " + held.GetItemCount("G3ASM_Klonfell"), 3, 255, 255, 180);
}
