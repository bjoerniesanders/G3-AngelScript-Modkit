// Selbsttest der g3asm-Skriptschnittstelle (laeuft ohne Spiel: g3asm_test <SharedBase.dll> tests\scripts\selftest.as)
// Jede check()-Bedingung muss wahr sein; das Testprogramm meldet die Zahl der Fehlschlaege.

bool near(float a, float b) { return abs(a - b) < 0.0001f; }

void TestString() {
    bCString s = "Gothic";
    check(s.GetLength() == 6, "GetLength");
    check(!s.IsEmpty(), "IsEmpty");
    bCString t = s + " 3";
    check(t == "Gothic 3", "opAdd/opEquals: " + t);
    t += "!";
    check(t == "Gothic 3!", "opAddAssign");
    check(t.Left(6) == "Gothic", "Left");
    check(t.Right(2) == "3!", "Right");
    check(t.Mid(7, 1) == "3", "Mid");
    check(t.Find("3") == 7, "Find");
    check(t.Contains("hic"), "Contains");
    check(!t.Contains("xyz"), "Contains negativ");
    check(s.GetUpper() == "GOTHIC", "GetUpper");
    check(s.GetLower() == "gothic", "GetLower");
    check(s.CompareNoCase("GOTHIC") == 0, "CompareNoCase");
    check(bCString("a") < bCString("b"), "opCmp");
    bCString n = "Wert: " + 42;
    check(n == "Wert: 42", "opAdd(int): " + n);
    check(ToString(1.5f) == "1.5", "ToString(float)");
    check(("x" + true) == "xtrue", "opAdd(bool)");
    bCString leer;
    check(leer.IsEmpty() && leer.GetLength() == 0, "leerer String");
    array<bCString> liste = {"Myrtana", "Nordmar", "Varant"};
    check(liste.length() == 3 && liste[1] == "Nordmar", "array<bCString>");
}

void TestVector() {
    bCVector v(3, 4, 0);
    check(near(v.GetMagnitude(), 5), "GetMagnitude");
    check(near(v.GetSquareMagnitude(), 25), "GetSquareMagnitude");
    bCVector n = v.GetNormalized();
    check(near(n.x, 0.6f) && near(n.y, 0.8f), "GetNormalized " + n.ToString());
    v.Normalize();
    check(near(v.GetMagnitude(), 1), "Normalize");
    bCVector x(1, 0, 0), y(0, 1, 0);
    bCVector z = x.GetCrossProduct(y);
    check(z == bCVector(0, 0, 1), "GetCrossProduct " + z.ToString());
    check(near(x.GetDotProduct(y), 0) && near(x.GetDotProduct(x), 1), "GetDotProduct");
    bCVector s = (x + y) * 2.0f - bCVector(1, 1, 0);
    check(s == bCVector(1, 1, 0), "Arithmetik " + s.ToString());
    check(-x == bCVector(-1, 0, 0), "opNeg");
    bCVector2 a(6, 8);
    check(near(a.GetMagnitude(), 10), "bCVector2::GetMagnitude");
    bCVector4 b(1, 2, 2, 4);
    check(near(b.GetMagnitude(), 5), "bCVector4::GetMagnitude");
}

void TestMatrix4() {
    bCMatrix4 m;
    check(near(m.GetElement(0, 0), 1) && near(m.GetElement(3, 3), 1) && near(m.GetElement(3, 0), 0), "bCMatrix4 Einheit");
    bCMatrix4 t;
    t.SetToTranslation(bCVector(1, 2, 3));
    bCVector o = t * bCVector(0, 0, 0);
    check(near(o.x, 1) && near(o.y, 2) && near(o.z, 3), "bCMatrix4 Verschiebung " + o.ToString());
    check(near(t.GetTranslation().z, 3), "bCMatrix4 GetTranslation");
    bCMatrix4 s;
    s.SetToScaling(bCVector(2, 3, 4));
    check(near(s.GetDeterminant(), 24), "bCMatrix4 Determinante " + s.GetDeterminant());
    bCMatrix4 r;
    r.SetToRotationAxisZ(3.14159265f / 2);
    bCMatrix4 rt = r * t;   // Drehung und Verschiebung
    bCMatrix4 inv = rt.GetInverted();
    bCMatrix4 id = rt * inv;
    bool ok = true;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) if (!near(id.GetElement(i, j), i == j ? 1.0f : 0.0f)) ok = false;
    check(ok, "bCMatrix4 M * M^-1 = Einheit");
    bCMatrix4 tr = rt;
    tr.Transpose();
    tr.Transpose();
    check(near(tr.GetElement(3, 0), rt.GetElement(3, 0)) && near(tr.GetElement(0, 1), rt.GetElement(0, 1)), "bCMatrix4 Transponieren zweimal");
    bCQuaternion q = r.GetRotation();
    check(near(q.GetMagnitude(), 1), "bCMatrix4 GetRotation normiert");
    Entity z;
    check(near(z.GetWorldMatrix().GetElement(0, 0), 1), "Entity ohne Spiel: Einheitsmatrix");
    check(!IsNPC(z) && !IsDynamicSpawn(z) && z.GetCategory() == "" && !z.IsBuilding(), "ohne Spiel kein NPC/keine Kategorie");
    check(STATUS_POISONED == 4 && STATUS_DISEASED == 8 && STATUS_BURNING == 1, "Konstanten der Statuseffekte");
    check(!z.GetSlotItem(SLOT_RIGHT_HAND).IsValid(), "Slot-Item ungueltiger Entity: ungueltig");
    check(GetProperty(z, "Item", "GoldValue") == "" && ListProperties(z, "Item") == "", "Eigenschaften ohne Spiel: leer");
    check(!GetTemplateEntity("Perk_OrcSlayer").IsValid() && !SetProperty(z, "Item", "GoldValue", "1"), "Vorlage/Schreiben ohne Spiel: ungueltig/false");
    check(!z.LearnSkill("Perk_1H_1") && !z.ActivateSkill("Perk_1H_1") && !z.DeactivateSkill("Perk_1H_1") && !z.HasLearnedSkill("Perk_1H_1") && !z.IsSkillActive("Perk_1H_1") && z.GetSkillActivationCount("Perk_1H_1") == -1, "Talente ohne Spiel: false/-1");
    check(z.DeleteItemStack("It_Perk_1H_1") == -1, "DeleteItemStack ohne Spiel: -1");
    check(!z.Kill(z) && !z.Kill() && z.HurtHitPoints(10) == -1 && !SetEntityRef(z, "gCInteraction_PS", "Owner", z), "Kill/HurtHitPoints/SetEntityRef ohne Spiel: false/-1");
    check(!EffectExists("eff_magic_summonlightning_target_01") && !StartEffect("eff_magic_summonlightning_target_01", z), "Effekte ohne Spiel: false");
    check(CountTemplateNames("It_") <= 0, "Vorlagen zaehlen ohne Spiel: -1");
    check(QUALITY_LIFESTEAL == 512 && QUALITY_SHARP == 32 && QUALITY_BLESSED == 64 && SLOT_RIGHT_HAND == 1 && SLOT_BACK_RIGHT == 6, "Konstanten der Qualitaet und Slots");
    check(z.GetSlotQuality(SLOT_RIGHT_HAND) == 0 && !z.SetSlotQuality(SLOT_RIGHT_HAND, QUALITY_LIFESTEAL), "Qualitaet ungueltiger Entity: 0/false");
    check(z.GetStatusEffects() == 0 && !z.HasStatusEffect(STATUS_DISEASED) && !z.SetStatusEffect(STATUS_DISEASED, true), "Statuseffekte ungueltiger Entity: 0/false");
    check(z.GetTemplateName() == "" && !z.GetParent().IsValid() && z.GetBoundsMax().GetMagnitude() < 0.001f, "ohne Spiel keine Vorlage/Huelle/Eltern");
    check(!PickCenter().IsValid() && !HasPickPoint() && GetPickPoint().GetMagnitude() < 0.001f, "ohne Spiel kein Bildschirm-Picking");
    check(!GetFocusedEntity().IsValid(), "ohne Spiel kein Fokus-Entity");
    check(!GetCrosshairEntity().IsValid(), "ohne Spiel kein Fadenkreuz-Entity/Zielpunkt");
}

void TestRotation() {
    bCMatrix3 m;
    check(near(m.GetElement(0, 0), 1) && near(m.GetElement(0, 1), 0), "Einheitsmatrix");
    m.SetToRotationAxisZ(3.14159265f / 2);
    bCVector r = m * bCVector(1, 0, 0);
    check(near(r.GetMagnitude(), 1), "Rotation erhaelt die Laenge " + r.ToString());
    check(near(abs(r.y), 1) && near(r.x, 0), "Rotation um Z um 90 Grad " + r.ToString());
    bCMatrix3 inv = m;
    inv.Invert();
    bCVector back = inv * r;
    check(near(back.x, 1) && near(back.y, 0), "Invert " + back.ToString());
    bCQuaternion q;
    q.SetQuaternion(m);
    check(near(q.GetMagnitude(), 1), "Quaternion aus Matrix normiert");
    bCMatrix3 m2;
    m2.SetToRotation(q);
    bCVector r2 = m2 * bCVector(1, 0, 0);
    check(near(r2.x, r.x) && near(r2.y, r.y), "Matrix -> Quaternion -> Matrix " + r2.ToString());
    bCQuaternion e;
    bCQuaternion p = q * e;
    check(near(p.x, q.x) && near(p.w, q.w), "Quaternion * Einheit");
}

void TestEntity() {
    Entity e;
    check(!e.IsValid(), "leere Entity ungueltig");
    check(e.GetName().IsEmpty() && e.GetPosition() == bCVector(0, 0, 0), "ungueltige Entity liefert Standardwerte");
    check(!e.SetPosition(bCVector(1, 2, 3)), "SetPosition auf ungueltiger Entity abgelehnt");
    check(!GetHero().IsValid(), "ohne Spiel kein Held");
    check(!FindEntity("PC_Hero").IsValid(), "ohne Spiel keine Suche");
    check(e.GetAttribute("STR") == -1 && e.GetAttributeBase("STR") == -1 && e.GetAttributeMax("HP") == -1, "Attribute ungueltiger Entity = -1");
    check(!e.AddAttributeBase("STR", 1) && !e.SetAttributeBase("STR", 5), "Attribut setzen auf ungueltiger Entity abgelehnt");
    check(GetGameDay() == -1 && GetGameHour() == -1 && GetGameMinute() == -1, "ohne Spiel keine Uhr");
    check(!SetGameTime(1, 12, 0), "ohne Spiel keine Uhr setzen");
    check(e.GetItemCount("It_Wolf_Fur") == -1 && e.RemoveItems("It_Wolf_Fur", 1) == -1 && e.AddItems("It_Wolf_Fur", 1) == -1, "ohne Spiel kein Inventar");
    check(e.GetInventoryStackCount() == -1 && e.GetInventoryItem(0).IsEmpty() && e.GetInventoryAmount(0) == -1 && e.GetInventoryAmount(-1) == -1,
          "Inventar aufzaehlen ohne Spiel: leer und -1");
    check(CloneTemplate("It_Wolf_Fur", "G3ASM_Klon") < 0, "ohne Spiel kein Template-Klonen");
    check(!SpawnNPC("Diego", bCVector(0, 0, 0)).IsValid() && FindTemplateNames("Diego") == "", "ohne Spiel kein SpawnNPC");
    { Entity z; check(!DestroyNPC(z) && !IsDynamicSpawn(z) && !z.LookAt(z) && !z.SetRotation(bCQuaternion()), "ohne Spiel kein DestroyNPC/LookAt/SetRotation"); }
    check(!QuestExists("X") && !CreateQuest("X") && !RunQuest("X") && !SucceedQuest("X") && !FailQuest("X") && !CloseQuest("X") && !RemoveQuest("X"), "ohne Spiel keine Quests");
    check(GetQuestStatus("X") == -1 && GetQuestXP("X") == -1 && !SetQuestStatus("X", QUEST_RUNNING) && !SetQuestXP("X", 5) && !ReloadQuestFiles(), "ohne Spiel keine Queststatus");
    check(!SetQuestTopic("X", "t") && !SetQuestDescription("X", "t") && AddQuestLogEntry("X", "h", "t") == -1 && !LoadQuestFile("X", "p") && GetQuestCount() == -1 && GetQuestNameAt(0).IsEmpty(), "ohne Spiel keine Quest-Texte");
    check(GetQuestDir().IsEmpty() && GetQuestFilePath("X").IsEmpty() && !GetGameDir().IsEmpty(), "Quest-Pfade ohne Spiel");
    check(GetInfoCount() == -1 && GetInfoName(0).IsEmpty() && GetInfoOwner(0).IsEmpty() && !DescribeInfo("X").IsEmpty(), "ohne Spiel keine Infos");
    check(!InfoExists("X") && !CreateInfo("X", "Y") && !RemoveInfo("X") && !SetInfoQuest("X", "q") && !SetInfoType("X", 3) && !SetInfoCondition("X", INFO_COND_GENERAL) &&
          !AddInfoLine("X", "Say", "npc", "", "", "", "t") && !RebuildInfos() && INFO_COND_RUNNING == 7 && INFO_TYPE_INFO == 3, "ohne Spiel keine Dialog-Erstellung");
    check(!SetQuestReward("X", "DEX", 1) && GetQuestReward("X").IsEmpty(), "ohne Spiel keine Quest-Belohnung");
    check(!SetGameEvent("X") && !ClearGameEvent("X") && !IsGameEventSet("X") && !SetInfoGiven("X", true) && !IsInfoGiven("X"), "ohne Spiel keine Schalter");
    check(QUEST_OPEN == 0 && QUEST_RUNNING == 1 && QUEST_SUCCESS == 2 && QUEST_FAILED == 3 && QUEST_WON == 7, "Quest-Konstanten");
}

// PackMod (ohne Spiel): Fehlerpfade; das eigentliche Schreiben prueft tests/packmod_roundtrip.py gegen echte Archive
void TestPackMod() {
    check(!PackFolder("Z:/gibt/es/nicht", "Z:/x.mod"), "PackFolder: fehlender Ordner schlaegt fehl");
    check(!GetPackError().IsEmpty(), "PackFolder: Fehlertext gesetzt");
    check(PackMod("Z:/gibt/es/nicht", "Quests").IsEmpty(), "PackMod: ohne Data-Ordner kein Ergebnis");
    check(NextModFile("a/b").IsEmpty() && NextModFile("").IsEmpty(), "NextModFile: ungueltiger Volumename");
}

void TestStdString() {
    string s = "Gothic";   // literal is bCString, converts to string
    check(s.length() == 6, "string: length");
    check(s == "Gothic", "string: opEquals mit Literal");
    check(s + " 3" == "Gothic 3" && "Ein " + s == "Ein Gothic", "string: opAdd mit Literal");
    check(s.substr(1, 3) == "oth" && s.findFirst("hic") == 3, "string: substr/findFirst");
    bCString b = s;   // string converts to bCString
    check(b == "Gothic" && b.GetLength() == 6, "string -> bCString");
    string zurueck = b;
    check(zurueck == s, "bCString -> string");
    check(bCString(s) == b && string(b) == s, "explizite Umwandlung");
    s += 3;
    check(s == "Gothic3", "string: opAddAssign(int)");
    Entity e;
    string item = "It_Wolf_Fur";
    check(e.GetItemCount(item) == -1, "string an Engine-Funktion mit bCString");
    array<string> teile = string("a,b,c").split(",");   // a literal is bCString: string methods need string(...)
    check(teile.length() == 3 && teile[1] == "b" && join(teile, "-") == "a-b-c", "string: split/join");
    check(parseInt("42") == 42 && formatInt(7, "0", 3) == "007", "string: parseInt/formatInt");
}

void TestDictionary() {
    dictionary d;
    d.set("gold", 100);
    d["name"] = bCString("Diego");
    d["text"] = string("Myrtana");
    int64 gold = 0;
    check(d.get("gold", gold) && gold == 100, "dictionary: set/get int");
    check(int(d["gold"]) == 100, "dictionary: opIndex int");
    int goldInt = 0;
    check(d.get("gold", goldInt) && goldInt == 100, "dictionary: get in int");
    float f = 0;
    check(d.get("gold", f) && f == 100, "dictionary: get in float");
    bCString name;
    check(d.get("name", name) && name == "Diego", "dictionary: bCString als Wert");
    string text;
    check(d.get("text", text) && text == "Myrtana", "dictionary: string als Wert");
    int fehlt;
    check(!d.get("fehlt", fehlt), "dictionary: get ohne Schluessel gibt false");
    check(d.exists("gold") && !d.exists("fehlt") && d.getSize() == 3, "dictionary: exists/getSize");
    bCString schluessel = "gold";
    check(d.exists(schluessel), "dictionary: bCString als Schluessel");
    Entity e;
    d.set("held", e);
    Entity e2;
    check(d.get("held", e2) && !e2.IsValid(), "dictionary: Entity als Wert");
    array<string>@ keys = d.getKeys();
    check(keys.length() == 4, "dictionary: getKeys");
    check(d.delete("gold") && !d.exists("gold") && d.getSize() == 3, "dictionary: delete");
    dictionary liste = {{"a", 1}, {"b", 2}};
    check(int(liste["b"]) == 2 && liste.getSize() == 2, "dictionary: Initialisierungsliste");
    d.deleteAll();
    check(d.isEmpty(), "dictionary: deleteAll");
}

// Coroutines: main() starts them, the test program then runs 60 ticks of 0.1 s, OnShutdown checks the result
int coCount = 0, coFrames = 0, coArgs = 0, coSelfStop = 0, coSelfId = 0, coEndlessId = 0, coLongId = 0, coStarts = 0;
void CoCount() { for (int i = 0; i < 3; i++) { Wait(0.25f); coCount++; } }
void CoFrames() { for (int i = 0; i < 5; i++) { Yield(); coFrames++; } }
void CoArgs(dictionary@ a) { int n; if (!a.get("n", n)) n = -1; Wait(0.1f); coArgs = n; }
void CoEndless() { while (true) {} }   // the 5-s stop must end it
void CoLong() { Wait(1000.0f); }       // still waits at the end: the reload must stop it
void CoStopSelf() { coSelfStop = 1; StopCoroutine(coSelfId); Yield(); coSelfStop = 2; }
void CoNothing() {}

void TestCoroutines() {
    check(StartCoroutine(CoCount) > 0 && StartCoroutine(CoFrames) > 0, "StartCoroutine: ID > 0");
    dictionary args = {{"n", 42}};
    check(StartCoroutine(CoArgs, args) > 0, "StartCoroutine mit dictionary");
    coEndlessId = StartCoroutine(CoEndless);
    coLongId = StartCoroutine(CoLong);
    coSelfId = StartCoroutine(CoStopSelf);
    check(coFrames == 0 && IsCoroutineRunning(coLongId), "Koroutine startet erst im naechsten Bild");
    int stopped = StartCoroutine(CoLong);
    check(StopCoroutine(stopped) && !IsCoroutineRunning(stopped) && !StopCoroutine(stopped), "StopCoroutine");
    check(!IsCoroutineRunning(99999) && !StopCoroutine(99999), "unbekannte Koroutinen-ID");
    for (int i = 0; i < 300; i++)
        if (StartCoroutine(CoNothing) > 0) coStarts++;
    check(coStarts == 250, "hoechstens 256 Koroutinen je Mod: " + coStarts);
}

// Hot reload: TestKeep() sets the globals, the test program loads the script again and calls AfterReload()
class Punkt { int x = 0; bCString name; }
int keepInt = 0;
float keepFloat = 0;
bool keepBool = false;
bCString keepText;
string keepStd;
array<int> keepArr;
array<bCString> keepNames;
dictionary keepDict;
Entity keepEnt;
bCVector keepVec;
Punkt keepObj;
Punkt@ keepRef;
Coroutine@ keepFn;
const int KEEP_CONST = 3;

void TestKeep() {
    keepInt = 7; keepFloat = 2.5f; keepBool = true;
    keepText = "Myrtana"; keepStd = "Nordmar";
    keepArr = {1, 2, 3}; keepNames = {"Diego", "Milten"};
    keepDict.set("gold", 100); keepDict["name"] = bCString("Gorn"); keepDict["std"] = string("Lester"); keepDict["vec"] = bCVector(1, 2, 3);
    keepVec = bCVector(4, 5, 6);
    keepObj.x = 4; keepObj.name = "Punkt A";
    @keepRef = Punkt(); keepRef.x = 9;
    @keepFn = CoNothing;
}

void AfterReload() {
    check(keepInt == 7 && keepFloat == 2.5f && keepBool, "Hot-Reload: Zahlen und bool behalten");
    check(keepText == "Myrtana" && keepStd == "Nordmar", "Hot-Reload: bCString und string behalten");
    check(keepArr.length() == 3 && keepArr[2] == 3 && keepNames.length() == 2 && keepNames[1] == "Milten", "Hot-Reload: Arrays behalten");
    int gold; bCString name; string s; bCVector v;
    check(keepDict.get("gold", gold) && gold == 100 && keepDict.get("name", name) && name == "Gorn", "Hot-Reload: dictionary mit Zahl und bCString");
    check(keepDict.get("std", s) && s == "Lester" && keepDict.get("vec", v) && v == bCVector(1, 2, 3), "Hot-Reload: dictionary mit string und bCVector");
    check(keepVec == bCVector(4, 5, 6) && !keepEnt.IsValid(), "Hot-Reload: POD-Typen behalten");
    check(keepObj.x == 4 && keepObj.name == "Punkt A", "Hot-Reload: Objekt einer Skriptklasse behalten");
    check(keepRef !is null && keepRef.x == 9, "Hot-Reload: Handle auf Skriptobjekt behalten");
    check(keepFn is null, "Hot-Reload: Funktions-Handle wird geleert");
    check(KEEP_CONST == 3 && frames == 1 && coCount == 3, "Hot-Reload: const und Zaehler aus dem ersten Lauf");
}

void main() {
    print("Selbsttest startet (API-Version " + GetApiVersion() + ")");
    TestString();
    TestStdString();
    TestDictionary();
    TestVector();
    TestRotation();
    TestMatrix4();
    TestPackMod();
    TestCoroutines();
    TestKeep();
    check(!SetOverrideArg(0, 1), "SetOverrideArg ausserhalb eines Overrides wird abgelehnt");
    check(AI_STATE == 1 && AI_FUNCTION == 2 && AI_CALLBACK == 3 && AI_ROUTINE == 4, "KI-Konstanten");
    check(GetLocText("G3ASM_Gibt_Es_Nicht").IsEmpty() && !IsModActive("gibts_nicht") && GetModVersion("gibts_nicht").IsEmpty() && GetSpeechText("INFO_A_B_German").IsEmpty(), "GetLocText/GetSpeechText/IsModActive ohne Spiel");
    check(DumpPropertySchema("../x.json") == -2 && DumpPropertySchema("a/b.json") == -2 && DumpPropertySchema("") == -2, "DumpPropertySchema: nur schlichte Dateinamen");
    TestEntity();
    print("Selbsttest fertig");
}

int frames = 0;
void OnFrame(float dt) { frames++; }
int items = 0, kills = 0, enters = 0;
void OnReceiveItem(Entity giver, Entity receiver, const bCString &in name, const bCString &in id, uint count) {
    items++;
    check(!giver.IsValid() && !receiver.IsValid(), "OnReceiveItem: ungueltige Entities ohne Spiel");
    check(name == "It_Test" && id == "GUID-123" && count == 7, "OnReceiveItem: Texte und Zahl");
}
void OnEnter(Entity e, const bCString &in area) { enters++; check(!e.IsValid() && area.IsEmpty(), "OnEnter: Nulltext kommt als leerer Text an"); }
void OnNPCKilled(Entity victim) { kills++; check(!victim.IsValid(), "OnNPCKilled: Entity ungueltig ohne Spiel"); }
int buys = 0, sells = 0;
void OnTrade(Entity trader, const bCString &in item, int amount, int gold, bool bought) {
    check(!trader.IsValid() && item == "It_Apple" && amount == 3 && gold == 12, "OnTrade: Haendler, Ware, Menge, Gold");
    if (bought) buys++; else sells++;
}
bool Override_Test(uint v) { check(v == 5, "Override_Test: Argument"); bool r = Super(); return !r; }
bool Override_TestCancel(uint v) { return true; }
bool Override_TestArg(uint v) {
    check(v == 5 && GetOverrideArg(0) == 5, "Override_TestArg: Argument");
    check(!SetOverrideArg(3, 1) && !SetOverrideArg(-1, 1), "SetOverrideArg: falscher Index wird abgelehnt");
    check(SetOverrideArg(0, v * 2) && GetOverrideArg(0) == 10, "SetOverrideArg: Wert gesetzt");
    return Super();
}
int keysDown = 0, keysUp = 0;
void OnKey(int vk, bool down) { check(vk == 0x7A, "OnKey: VK-Code"); if (down) keysDown++; else keysUp++; }
void OnShutdown() {
    check(frames == 1, "OnFrame genau einmal aufgerufen");
    check(keysDown == 1 && keysUp == 1, "OnKey: je einmal gedrueckt und losgelassen");
    check(items == 1 && kills == 1 && enters == 1, "Spielereignisse genau einmal zugestellt");
    check(buys == 1 && sells == 1, "OnTrade: bool kommt als true und false an");
    check(coCount == 3 && coFrames == 5 && coArgs == 42, "Koroutinen: Wait, Yield und dictionary-Argument");
    check(coSelfStop == 1, "Koroutine stoppt sich selbst mit StopCoroutine");
    check(!IsCoroutineRunning(coEndlessId), "Endlosschleife in Koroutine nach 5 s abgebrochen");
    check(IsCoroutineRunning(coLongId), "lange Wartezeit laeuft bis zum Entladen");
}
