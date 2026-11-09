// Zeichen-API (Skript-Schnittstelle ab Version 34, Roadmap 7.2).
// Gemessen 2026-11-09:
//  - eCGUIAdmin::EndFrame (Export) laeuft im Spiel praktisch nie (4-mal in 10 s, nur im Menue): GFC ruft den Renderer ueber dessen eigene vtable.
//  - eCGUIAdmin::CreateTexture(bCByteAlphaColor) ist ein leerer Rumpf (gibt immer -1 zurueck) -> keine einfarbigen Rechtecke ueber die GUI.
//  - Die Kameramatrix (eCCameraBase::GetWorldToScreenMatrix, Zeilenvektor * M, / w) liefert Pixel des Spielfensters.
// Deshalb wie die Anzeige-Plugins der Steam-Installation: Umleitung von GFC.dll CFFGFCWnd::OnPaint. Malt das Desktop-Fenster (oder, falls es nie selbst
// malt, das erste Fenster eines Spielbilds), ruft der Patch nach dem Original das Ereignis OnDraw() aller Mods. Gezeichnet wird ueber den Geraetekontext
// des Desktops (CFFGFCWnd::GetDesktopWindow()->GetDC(), CFFGFCDeviceContext::DrawTextA/CalcTextRect); Schriften legt eCGUIAdmin::CreateFontA an
// (Name, Hoehe, Breite, -, fett, kursiv; fett = Gewicht 700).
#include <windows.h>

#include <angelscript.h>

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace og3p {
namespace {

struct Rect {
    int left, top, right, bottom;
};
struct Mat4 {
    float m[4][4];
};
struct FColor {   // bCFloatAlphaColor: vtable, r, g, b, a (20 Byte, SDK); per Konstruktor der SharedBase gebaut
    void* vt;
    float r, g, b, a;
};

typedef void(__thiscall* PaintFn)(void*);
typedef void*(__thiscall* WndGetFn)(const void*);
typedef void(__thiscall* ClientRectFn)(const void*, Rect*);
typedef bool(__thiscall* DcTextFn)(void*, unsigned, const char*, int, Rect*, unsigned, FColor, FColor*);
typedef int(__thiscall* DcCalcFn)(void*, unsigned, const char*, int, Rect*, unsigned);
typedef unsigned(__thiscall* CreateFontFn)(void*, const char*, int, int, bool, bool, bool);
typedef void*(__stdcall* InstFn)();
typedef void*(__thiscall* GetAdminFn)(void*);
typedef void*(__thiscall* GetCamFn)(void*);
typedef Mat4*(__thiscall* W2SFn)(const void*, Mat4*);
typedef void*(__thiscall* ColorCtorFn)(void*, float, float, float, float);

struct Api {
    bool tried = false;
    WndGetFn desktop = nullptr, getDC = nullptr;
    ClientRectFn clientRect = nullptr;
    DcTextFn dcText = nullptr;
    DcCalcFn dcCalc = nullptr;
    CreateFontFn createFont = nullptr;
    InstFn guiModule = nullptr, app = nullptr;
    GetAdminFn guiAdmin = nullptr;
    GetCamFn getCam = nullptr;
    W2SFn w2s = nullptr;
    ColorCtorFn colorCtor = nullptr;
} g_a;

ScriptHost* g_host = nullptr;
PaintFn o_paint = nullptr;
void* g_dc = nullptr;   // Geraetekontext des Desktops, nur waehrend OnDraw gesetzt
bool g_inDraw = false;
DWORD g_gameThread = 0;
int g_screenW = 0, g_screenH = 0;   // Zeichenflaeche (Client-Rechteck des Desktops)
int g_clientW = 0, g_clientH = 0;   // Spielfenster in Pixeln (darin rechnet die Kameramatrix)
std::map<std::string, unsigned> g_fonts;
bool g_outsideLogged = false;

// Bilderkennung: OnPaint laeuft fuer jedes Fenster; je Spielbild (DrawFrameTick aus HProcess) hoechstens einmal OnDraw
unsigned g_tick = 0, g_drawnTick = ~0u;
bool g_desktopPaints = false;   // malt der Desktop selbst (dann nur bei ihm zeichnen)
// Messung der ersten 20 s im Spiel
unsigned g_paints = 0, g_desktopCalls = 0, g_draws = 0;
DWORD g_measureStart = 0;
bool g_measured = false;

void Resolve() {
    if (g_a.tried) return;
    g_a.tried = true;
    HMODULE gfc = GetModuleHandleA("GFC.dll");
    HMODULE eng = GetModuleHandleA("Engine.dll");
    HMODULE sb = GetModuleHandleA("SharedBase.dll");
    if (gfc) {
        g_a.desktop = reinterpret_cast<WndGetFn>(GetProcAddress(gfc, "?GetDesktopWindow@CFFGFCWnd@@QBEPAV1@XZ"));
        g_a.getDC = reinterpret_cast<WndGetFn>(GetProcAddress(gfc, "?GetDC@CFFGFCWnd@@QBEPAVCFFGFCDeviceContext@@XZ"));
        g_a.clientRect = reinterpret_cast<ClientRectFn>(GetProcAddress(gfc, "?GetClientRect@CFFGFCWnd@@QBEXAAVbCRect@@@Z"));
        g_a.dcText = reinterpret_cast<DcTextFn>(GetProcAddress(gfc, "?DrawTextA@CFFGFCDeviceContext@@QAE_NKPBDHPAVbCRect@@KVbCFloatAlphaColor@@PAV3@@Z"));
        g_a.dcCalc = reinterpret_cast<DcCalcFn>(GetProcAddress(gfc, "?CalcTextRect@CFFGFCDeviceContext@@QAEHKPBDHAAVbCRect@@K@Z"));
    }
    if (eng) {
        g_a.createFont = reinterpret_cast<CreateFontFn>(GetProcAddress(eng, "?CreateFontA@eCGUIAdmin@@UAEKPBDHH_N11@Z"));
        g_a.guiModule = reinterpret_cast<InstFn>(GetProcAddress(eng, "?GetInstance@eCGUIModule@@SGAAV1@XZ"));
        g_a.guiAdmin = reinterpret_cast<GetAdminFn>(GetProcAddress(eng, "?GetAdmin@eCGUIModule@@QAEAAVeCGUIAdmin@@XZ"));
        g_a.app = reinterpret_cast<InstFn>(GetProcAddress(eng, "?GetInstance@eCApplication@@SGAAV1@XZ"));
        g_a.getCam = reinterpret_cast<GetCamFn>(GetProcAddress(eng, "?GetCurrentCamera@eCApplication@@QAEAAVeCCameraBase@@XZ"));
        g_a.w2s = reinterpret_cast<W2SFn>(GetProcAddress(eng, "?GetWorldToScreenMatrix@eCCameraBase@@QBE?AVbCMatrix@@XZ"));
    }
    if (sb) g_a.colorCtor = reinterpret_cast<ColorCtorFn>(GetProcAddress(sb, "??0bCFloatAlphaColor@@QAE@MMMM@Z"));
}

void* SafeDesktop(void* wnd) {
    __try {
        return g_a.desktop(wnd);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeDC(void* wnd) {
    __try {
        return g_a.getDC(wnd);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
bool SafeClient(void* wnd, Rect* r) {
    __try {
        g_a.clientRect(wnd, r);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

void UpdateSizes(void* desktop) {
    Rect r{0, 0, 0, 0};
    if (desktop && g_a.clientRect && SafeClient(desktop, &r)) {
        g_screenW = r.right - r.left;
        g_screenH = r.bottom - r.top;
    }
    if (HWND w = GameWindow()) {
        RECT c;
        if (GetClientRect(w, &c)) {
            g_clientW = c.right - c.left;
            g_clientH = c.bottom - c.top;
        }
    }
}

void LogMeasure() {
    char b[260];
    std::snprintf(b, sizeof b,
                  "Zeichnen: in 20 s %u OnPaint-Aufrufe, davon %u vom Desktop selbst, %u-mal OnDraw (%.1f je s); Zeichenflaeche %dx%d, Spielfenster %dx%d",
                  g_paints, g_desktopCalls, g_draws, g_draws / 20.0, g_screenW, g_screenH, g_clientW, g_clientH);
    if (g_host) g_host->Log(b);
}

void __fastcall HPaint(void* wnd, void*) {
    o_paint(wnd);
    const DWORD tid = GetCurrentThreadId();
    if (!g_host || !g_a.desktop || !g_a.getDC || g_inDraw) return;
    if (g_gameThread && tid != g_gameThread) return;
    void* desk = SafeDesktop(wnd);
    if (!desk) return;
    const bool isDesk = desk == wnd;
    if (!g_measured && g_tick) {
        if (!g_measureStart) g_measureStart = GetTickCount();
        ++g_paints;
        if (isDesk) ++g_desktopCalls;
    }
    if (isDesk) g_desktopPaints = true;
    // zeichnen: beim Desktop, sonst (falls der Desktop nie selbst malt) beim ersten Fenster des Bildes
    if (g_desktopPaints ? !isDesk : g_drawnTick == g_tick) return;
    g_drawnTick = g_tick;
    void* dc = SafeDC(desk);
    if (!dc) return;
    static DWORD lastSize = 0;
    if (GetTickCount() - lastSize > 1000u) {
        lastSize = GetTickCount();
        UpdateSizes(desk);
    }
    g_dc = dc;
    g_inDraw = true;
    g_host->CallSimple("void OnDraw()");
    g_inDraw = false;
    g_dc = nullptr;
    if (!g_measured && g_measureStart) {
        ++g_draws;
        if (GetTickCount() - g_measureStart >= 20000u) {
            g_measured = true;
            LogMeasure();
        }
    }
}

bool CanDraw(const char* what) {
    if (g_inDraw && g_dc) return true;
    if (!g_outsideLogged && ScriptHost::s_current) {
        g_outsideLogged = true;
        ScriptHost::s_current->Log(std::string(what) + ": nur innerhalb von OnDraw() (weitere Meldungen unterdrueckt)");
    }
    return false;
}

// ---- rohe Aufrufe mit SEH
unsigned SafeCreateFont(const char* face, int h, bool bold, bool italic) {
    __try {
        void* gm = g_a.guiModule ? g_a.guiModule() : nullptr;
        void* admin = gm && g_a.guiAdmin ? g_a.guiAdmin(gm) : nullptr;
        return admin && g_a.createFont ? g_a.createFont(admin, face, h, 0, false, bold, italic) : 0;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool SafeColor(FColor* c, unsigned argb) {
    __try {
        g_a.colorCtor(c, float((argb >> 16) & 0xFF) / 255.0f, float((argb >> 8) & 0xFF) / 255.0f, float(argb & 0xFF) / 255.0f,
                      float((argb >> 24) & 0xFF) / 255.0f);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeDcText(void* dc, unsigned font, const char* t, Rect* r, unsigned flags, const FColor* c) {
    __try {
        return g_a.dcText(dc, font, t, -1, r, flags, *c, nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int SafeDcCalc(void* dc, unsigned font, const char* t, Rect* r, unsigned flags) {
    __try {
        return g_a.dcCalc(dc, font, t, -1, r, flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool SafeW2S(Mat4* out) {
    __try {
        void* app = g_a.app ? g_a.app() : nullptr;
        void* cam = app && g_a.getCam ? g_a.getCam(app) : nullptr;
        if (!cam || !g_a.w2s) return false;
        g_a.w2s(cam, out);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

// ---- Bindings
const unsigned kNoClip = 0x100;   // DT_NOCLIP

int CreateFontScript(const SStr& face, int height, bool bold, bool italic) {
    if (!g_a.createFont) return 0;
    char key[160];
    std::snprintf(key, sizeof key, "%s|%d|%d|%d", face.c_str(), height, bold ? 1 : 0, italic ? 1 : 0);
    auto it = g_fonts.find(key);
    if (it != g_fonts.end()) return int(it->second);
    const unsigned f = SafeCreateFont(face.c_str(), height, bold, italic);
    g_fonts[key] = f;
    if (ScriptHost::s_current) {
        char b[200];
        std::snprintf(b, sizeof b, "Zeichnen: Schrift '%s' %d%s%s -> Handle %u", face.c_str(), height, bold ? " fett" : "", italic ? " kursiv" : "", f);
        ScriptHost::s_current->Log(b);
    }
    return int(f);
}
bool DrawOne(int font, int x, int y, const SStr& text, unsigned argb) {
    FColor c;
    if (!g_a.colorCtor || !SafeColor(&c, argb)) return false;
    Rect r{x, y, x + 4000, y + 1000};
    return SafeDcText(g_dc, unsigned(font), text.c_str(), &r, kNoClip, &c);
}
bool DrawTextScript(int font, int x, int y, const SStr& text, unsigned argb, bool shadow) {
    if (!CanDraw("DrawText") || !g_a.dcText) return false;
    if (shadow) DrawOne(font, x + 1, y + 1, text, argb & 0xFF000000u);
    return DrawOne(font, x, y, text, argb);
}
int TextWidthScript(int font, const SStr& text) {
    if (!CanDraw("TextWidth") || !g_a.dcCalc) return 0;
    Rect r{0, 0, 4000, 1000};
    SafeDcCalc(g_dc, unsigned(font), text.c_str(), &r, kNoClip);
    return r.right - r.left;
}
int ScreenWidth() { return g_screenW; }
int ScreenHeight() { return g_screenH; }

// Weltpunkt -> Zeichenflaeche; false, wenn der Punkt hinter der Kamera liegt oder keine Kamera da ist
bool WorldToScreenScript(const SVec3& p, float& sx, float& sy) {
    Mat4 m;
    if (!SafeW2S(&m) || g_clientW <= 0 || g_clientH <= 0) return false;
    const float x = p.x * m.m[0][0] + p.y * m.m[1][0] + p.z * m.m[2][0] + m.m[3][0];
    const float y = p.x * m.m[0][1] + p.y * m.m[1][1] + p.z * m.m[2][1] + m.m[3][1];
    const float w = p.x * m.m[0][3] + p.y * m.m[1][3] + p.z * m.m[2][3] + m.m[3][3];
    if (w <= 0.0001f) return false;
    sx = x / w * float(g_screenW) / float(g_clientW);
    sy = y / w * float(g_screenH) / float(g_clientH);
    return true;
}

}   // namespace

bool RegisterDrawBindings(asIScriptEngine* e) {
    bool ok = true;
    OG3_REG(e->RegisterGlobalFunction("int CreateFont(const bCString &in face, int height, bool bold = false, bool italic = false)",
                                      asFUNCTION(CreateFontScript), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction(
        "bool DrawText(int font, int x, int y, const bCString &in text, uint argb = 0xFFFFFFFF, bool shadow = true)", asFUNCTION(DrawTextScript),
        asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int TextWidth(int font, const bCString &in text)", asFUNCTION(TextWidthScript), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetScreenWidth()", asFUNCTION(ScreenWidth), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("int GetScreenHeight()", asFUNCTION(ScreenHeight), asCALL_CDECL));
    OG3_REG(e->RegisterGlobalFunction("bool WorldToScreen(const bCVector &in pos, float &out x, float &out y)", asFUNCTION(WorldToScreenScript),
                                      asCALL_CDECL));
    return ok;
}

// einmal je Spielbild aus HProcess (Spiel-Thread): Bildzaehler fuer "hoechstens einmal OnDraw je Bild"
void DrawFrameTick() {
    if (!g_gameThread) g_gameThread = GetCurrentThreadId();
    ++g_tick;
}

int InstallDrawHooks(ScriptHost& host) {
    g_host = &host;
    Resolve();
    HMODULE gfc = GetModuleHandleA("GFC.dll");
    void* target = gfc ? reinterpret_cast<void*>(GetProcAddress(gfc, "?OnPaint@CFFGFCWnd@@UAEXXZ")) : nullptr;
    const bool ok = target && MH_CreateHook(target, reinterpret_cast<void*>(&HPaint), reinterpret_cast<void**>(&o_paint)) == MH_OK &&
                    MH_EnableHook(target) == MH_OK;
    char b[220];
    std::snprintf(b, sizeof b, "Zeichnen: OnPaint-Hook (GFC.dll) %s; Text %s, Schrift %s, Farbe %s, Kamera %s", ok ? "aktiv" : "FEHLT",
                  g_a.dcText ? "ja" : "nein", g_a.createFont ? "ja" : "nein", g_a.colorCtor ? "ja" : "nein", g_a.w2s && g_a.getCam ? "ja" : "nein");
    host.Log(b);
    return ok ? 1 : 0;
}

}   // namespace og3p
