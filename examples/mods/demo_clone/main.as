// Template clone with a fixed ID: makes the template "G3ASM_Klonfell" (copy of It_Wolf_Fur) again at the start and at each load. Its ID is a
// hash of the name, thus it is the same at each game start: cloned items in a saved game find the template again after a new start.
// Numeric keypad 0: puts one item into the inventory of the hero. Log: g3asm.log.
bool Sicherstellen(const bCString &in wo) {
    int rc = CloneTemplate("It_Wolf_Fur", "G3ASM_Klonfell");   // 0 made, 1 already existed, <0 error (for example no game yet)
    RegisterText("FO_G3ASM_Klonfell", "Geklontes Wolfsfell");   // name and description: string table FO_<template>, ITEMDESC_<template>
    RegisterText("ITEMDESC_G3ASM_Klonfell", "Dieses Fell wurde zur Laufzeit aus einer Vorlage geklont.");
    print("Klonvorlage (" + wo + "): rc=" + rc);
    return rc >= 0;
}

void main() { Sicherstellen("main"); }
void OnQuestsLoaded() { Sicherstellen("OnQuestsLoaded"); }   // before the game reads the saved game
void OnWorldActivate() { Sicherstellen("OnWorldActivate"); }

void OnKey(int vk, bool down) {
    if (!down || vk != 0x60) return;
    Sicherstellen("Taste");
    Entity held = GetHero();
    int add = held.AddItems("G3ASM_Klonfell", 1);
    print("Klon AddItems=" + add + ", Anzahl G3ASM_Klonfell=" + held.GetItemCount("G3ASM_Klonfell"));
    PrintScreen("Klonfell: " + held.GetItemCount("G3ASM_Klonfell"), 3, 255, 255, 180);
}
