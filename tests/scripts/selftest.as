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
    check(!SetQuestTopic("X", "t") && !SetQuestDescription("X", "t") && !LoadQuestFile("X", "p") && GetQuestCount() == -1 && GetQuestNameAt(0).IsEmpty(), "ohne Spiel keine Quest-Texte");
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

void main() {
    print("Selbsttest startet (API-Version " + GetApiVersion() + ")");
    TestString();
    TestVector();
    TestRotation();
    TestMatrix4();
    TestPackMod();
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
}
