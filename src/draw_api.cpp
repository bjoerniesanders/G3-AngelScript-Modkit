// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
// Drawing API (script interface from version 34, images from 38, drawing above the HUD from 39, roadmap 7.2).
// Measured 2026-11-09:
//  - eCGUIAdmin::EndFrame (export) almost never runs in the game. eCGUIAdmin::CreateTexture(color) and eCGUIRenderer::DrawRect are empty bodies
//    -> no rectangles with only one color.
//  - The camera matrix (eCCameraBase::GetWorldToScreenMatrix, row vector * M, / w) gives pixels of the game window.
//  - Device context of the GFC windows (CFFGFCWnd::GetDC): the windows collect their drawing commands and send them when they paint. If you draw at the
//    first window of a frame (OnPaint), everything is BELOW the HUD. At the end of the frame, the context has no effect at all.
//  - eCGUIRenderer::EndFrame runs 2 times for each frame on two renderers. The second one is the main renderer (eCGUIAdmin::GetRenderer), which also paints
//    the mouse pointer and the compass needle last (their transformation still applies: reset it before you draw). If you draw directly through it immediately before its EndFrame,
//    everything is ABOVE all windows (also above the inventory).
// Thus: OnDraw() runs at the end of the frame of the main renderer. DrawText/TextWidth go to eCGUIRenderer::DrawTextA/CalcTextRect, DrawImage goes to
// eCGUIRenderer::DrawSprite (texture + part from eCGUIAdmin::LoadBitmapA). The renderer itself scales the GUI area (GetScreenWidth/Height, for example 1365x768) to the
// window (its drawing scale, measured 1.875; an own scaling through the transformation doubles it).
// If the end of the frame does not come (for 2 frames), the modkit draws as before at the first window through the device context of the desktop (CFFGFCDeviceContext::DrawTextA/CalcTextRect/DrawBitmap).
// eCGUIAdmin::CreateFontA makes fonts (name, height, width, -, bold, italic; bold = weight 700). The two paths use the same numbers.
// Images: names as in g3.gui.res ("\data\gui\icons\hud\v3\icons\misc\g3_hud_icon_misc_compass.tga") or only the file name. Fallback path: CFFGFCBitmap
// (8 bytes: vtable, IFFGUIBitmap*) ::Create(name); CFFGFCDeviceContext::DrawBitmap(image, bCRect*, flags, angle in radians - Game.dll turns the
// compass needle in this way).
#include <windows.h>

#include <angelscript.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <MinHook.h>

#include "sb_api.h"
#include "script_host.h"

namespace g3asm {
namespace {

struct Rect {
    int left, top, right, bottom;
};
struct Mat4 {
    float m[4][4];
};
struct Vec3 {
    float x, y, z;
};
struct FColor {   // bCFloatAlphaColor: vtable, r, g, b, a (20 bytes, SDK); built with the constructor of SharedBase
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
typedef void*(__thiscall* BmpCtorFn)(void*);
typedef bool(__thiscall* BmpCreateFn)(void*, const void*);
typedef bool(__thiscall* DcBitmapFn)(void*, void*, const Rect*, unsigned, float);
// Renderer (Engine eCGUIRenderer)
typedef bool(__thiscall* EndFrameFn)(void*, bool);
typedef bool(__thiscall* RTextFn)(void*, unsigned, const char*, int, Rect*, unsigned, unsigned);
typedef int(__thiscall* RCalcFn)(void*, unsigned, const char*, int, Rect*, unsigned);
typedef bool(__thiscall* RSpriteFn)(void*, unsigned, const Rect*, const Vec3*, const Vec3*, const Mat4*, unsigned);
typedef bool(__thiscall* RClipFn)(void*, const Rect*);
typedef bool(__thiscall* RXformFn)(void*, const Mat4*);
typedef bool(__thiscall* RScaleFn)(void*, Vec3*);
typedef void*(__thiscall* AdminRendererFn)(void*);
typedef bool(__thiscall* LoadBmpFn)(void*, const void*, void*);

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
    BmpCtorFn bmpCtor = nullptr;
    BmpCreateFn bmpCreate = nullptr;
    DcBitmapFn dcBitmap = nullptr;
    RTextFn rText = nullptr;
    RCalcFn rCalc = nullptr;
    RSpriteFn rSprite = nullptr;
    RClipFn rClip = nullptr;
    RXformFn rXform = nullptr;
    RScaleFn rScale = nullptr;
    AdminRendererFn adminRenderer = nullptr;
    LoadBmpFn loadBmp = nullptr;
} g_a;

ScriptHost* g_host = nullptr;
PaintFn o_paint = nullptr;
EndFrameFn o_endFrame = nullptr;
// during OnDraw: exactly one of the two paths is set
void* g_dc = nullptr;     // fallback: device context of the desktop
void* g_rend = nullptr;   // main renderer at the end of the frame
float g_rendScale = 0.0f;   // drawing scale of the renderer (only for the log; it scales itself)
bool g_inDraw = false;
DWORD g_gameThread = 0;
int g_screenW = 0, g_screenH = 0;   // drawing area (client rectangle of the desktop)
int g_clientW = 0, g_clientH = 0;   // game window in pixels (the camera matrix calculates in it)
std::map<std::string, unsigned> g_fonts;
bool g_outsideLogged = false;

// frame detection: maximum 1 OnDraw for each game frame (DrawFrameTick from HProcess)
unsigned g_tick = 0, g_drawnTick = ~0u;
unsigned g_rendTick = ~0u;   // last frame in which the renderer path drew
void* g_desk = nullptr;      // desktop window (kept from OnPaint)
void* g_mainRend = nullptr;
// measurement of the first 20 s in the game
unsigned g_paints = 0, g_draws = 0, g_rendDraws = 0, g_endFrames = 0;
DWORD g_measureStart = 0;
bool g_measured = false;
bool g_scaleLogged = false;

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
        g_a.bmpCtor = reinterpret_cast<BmpCtorFn>(GetProcAddress(gfc, "??0CFFGFCBitmap@@QAE@XZ"));
        g_a.bmpCreate = reinterpret_cast<BmpCreateFn>(GetProcAddress(gfc, "?Create@CFFGFCBitmap@@QAE_NABVbCString@@@Z"));
        g_a.dcBitmap = reinterpret_cast<DcBitmapFn>(GetProcAddress(gfc, "?DrawBitmap@CFFGFCDeviceContext@@QAE_NPAVCFFGFCBitmap@@PBVbCRect@@KM@Z"));
    }
    if (eng) {
        auto E = [eng](const char* n) { return reinterpret_cast<void*>(GetProcAddress(eng, n)); };
        g_a.createFont = reinterpret_cast<CreateFontFn>(E("?CreateFontA@eCGUIAdmin@@UAEKPBDHH_N11@Z"));
        g_a.guiModule = reinterpret_cast<InstFn>(E("?GetInstance@eCGUIModule@@SGAAV1@XZ"));
        g_a.guiAdmin = reinterpret_cast<GetAdminFn>(E("?GetAdmin@eCGUIModule@@QAEAAVeCGUIAdmin@@XZ"));
        g_a.app = reinterpret_cast<InstFn>(E("?GetInstance@eCApplication@@SGAAV1@XZ"));
        g_a.getCam = reinterpret_cast<GetCamFn>(E("?GetCurrentCamera@eCApplication@@QAEAAVeCCameraBase@@XZ"));
        g_a.w2s = reinterpret_cast<W2SFn>(E("?GetWorldToScreenMatrix@eCCameraBase@@QBE?AVbCMatrix@@XZ"));
        g_a.rText = reinterpret_cast<RTextFn>(E("?DrawTextA@eCGUIRenderer@@UAE_NKPBDHPAVbCRect@@KK@Z"));
        g_a.rCalc = reinterpret_cast<RCalcFn>(E("?CalcTextRect@eCGUIRenderer@@UAEHKPBDHPAVbCRect@@K@Z"));
        g_a.rSprite = reinterpret_cast<RSpriteFn>(E("?DrawSprite@eCGUIRenderer@@UAE_NKPBVbCRect@@PBVbCVector@@1PBVbCMatrix@@K@Z"));
        g_a.rClip = reinterpret_cast<RClipFn>(E("?SetClipRect@eCGUIRenderer@@UAE_NPBVbCRect@@@Z"));
        g_a.rXform = reinterpret_cast<RXformFn>(E("?SetTransform@eCGUIRenderer@@UAE_NPBVbCMatrix@@@Z"));
        g_a.rScale = reinterpret_cast<RScaleFn>(E("?GetDrawScale@eCGUIRenderer@@UBE_NAAVbCVector@@@Z"));
        g_a.adminRenderer = reinterpret_cast<AdminRendererFn>(E("?GetRenderer@eCGUIAdmin@@QAEAAVeCGUIRenderer@@XZ"));
        g_a.loadBmp = reinterpret_cast<LoadBmpFn>(E("?LoadBitmapA@eCGUIAdmin@@UAE_NABVbCString@@AAUSFFGUIBitmap@@@Z"));
    }
    if (sb) g_a.colorCtor = reinterpret_cast<ColorCtorFn>(GetProcAddress(sb, "??0bCFloatAlphaColor@@QAE@MMMM@Z"));
}
bool RendererReady() { return g_a.rText && g_a.rCalc && g_a.rXform && g_a.adminRenderer; }

// ---- raw calls with SEH
void* SafeAdmin() {
    __try {
        void* gm = g_a.guiModule ? g_a.guiModule() : nullptr;
        return gm && g_a.guiAdmin ? g_a.guiAdmin(gm) : nullptr;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
}
void* SafeMainRenderer() {
    void* admin = SafeAdmin();
    if (!admin || !g_a.adminRenderer) return nullptr;
    __try {
        return g_a.adminRenderer(admin);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
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
unsigned SafeCreateFont(const char* face, int h, bool bold, bool italic) {
    void* admin = SafeAdmin();
    if (!admin || !g_a.createFont) return 0;
    __try {
        return g_a.createFont(admin, face, h, 0, false, bold, italic);
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
bool SafeRText(void* r, unsigned font, const char* t, Rect* rc, unsigned flags, unsigned argb) {
    __try {
        return g_a.rText(r, font, t, -1, rc, flags, argb);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
int SafeRCalc(void* r, unsigned font, const char* t, Rect* rc, unsigned flags) {
    __try {
        return g_a.rCalc(r, font, t, -1, rc, flags);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool SafeRXform(void* r, const Mat4* m) {
    __try {
        return g_a.rXform(r, m);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeRPrepare(void* r, Vec3* scale) {
    __try {
        if (g_a.rScale) g_a.rScale(r, scale);
        Rect clip{0, 0, 16384, 16384};
        if (g_a.rClip) g_a.rClip(r, &clip);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeRSprite(void* r, unsigned tex, const Rect* src, const Mat4* m) {
    __try {
        return g_a.rSprite(r, tex, src, nullptr, nullptr, m, 0xFFFFFFFFu);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
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

Mat4 Identity() {
    Mat4 m = {};
    m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0f;
    return m;
}
Mat4 Mul(const Mat4& a, const Mat4& b) {   // row vectors: first a, then b
    Mat4 r = {};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k) r.m[i][j] += a.m[i][k] * b.m[k][j];
    return r;
}
Mat4 Scale(float x, float y) {
    Mat4 m = Identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    return m;
}
Mat4 Translate(float x, float y) {
    Mat4 m = Identity();
    m.m[3][0] = x;
    m.m[3][1] = y;
    return m;
}
Mat4 Rotate(float a) {
    Mat4 m = Identity();
    const float c = std::cos(a), s = std::sin(a);
    m.m[0][0] = c;
    m.m[0][1] = s;
    m.m[1][0] = -s;
    m.m[1][1] = c;
    return m;
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
    char b[360];
    std::snprintf(b, sizeof b,
                  "Zeichnen: in 20 s %u OnPaint-Aufrufe, %u Bildenden des Haupt-Renderers, %u-mal OnDraw (%.1f je s, davon %u ueber dem HUD); "
                  "Zeichenflaeche %dx%d, Spielfenster %dx%d, Massstab des Renderers %.3f",
                  g_paints, g_endFrames, g_draws, g_draws / 20.0, g_rendDraws, g_screenW, g_screenH, g_clientW, g_clientH, g_rendScale);
    if (g_host) g_host->Log(b);
}

void CountDraw(bool renderer) {
    if (g_measured || !g_measureStart) return;
    ++g_draws;
    if (renderer) ++g_rendDraws;
    if (GetTickCount() - g_measureStart >= 20000u) {
        g_measured = true;
        LogMeasure();
    }
}

void SizesOncePerSecond() {
    static DWORD lastSize = 0;
    if (g_desk && GetTickCount() - lastSize > 1000u) {
        lastSize = GetTickCount();
        UpdateSizes(g_desk);
    }
}

// fallback: OnDraw through the device context of the desktop at the first window of a frame (below the HUD)
void __fastcall HPaint(void* wnd, void*) {
    o_paint(wnd);
    if (!g_host || !g_a.desktop || !g_a.getDC || g_inDraw) return;
    if (g_gameThread && GetCurrentThreadId() != g_gameThread) return;
    void* desk = SafeDesktop(wnd);
    if (!desk) return;
    g_desk = desk;
    if (!g_measured && g_tick) {
        if (!g_measureStart) g_measureStart = GetTickCount();
        ++g_paints;
    }
    if (g_rendTick != ~0u && g_tick - g_rendTick <= 2u) return;   // the renderer path draws
    if (g_drawnTick == g_tick) return;
    void* dc = SafeDC(desk);
    if (!dc) return;
    g_drawnTick = g_tick;
    SizesOncePerSecond();
    g_dc = dc;
    g_inDraw = true;
    g_host->CallSimple("void OnDraw()");
    g_inDraw = false;
    g_dc = nullptr;
    CountDraw(false);
}

// end of the frame of the main renderer: OnDraw above all windows
bool __fastcall HEndFrame(void* r, void*, bool flag) {
    if (g_host && !g_inDraw && g_desk && RendererReady() && (!g_gameThread || GetCurrentThreadId() == g_gameThread)) {
        g_mainRend = SafeMainRenderer();   // each time: the game can make the renderer again (device reset, resolution)
        if (r == g_mainRend && g_rendTick != g_tick) {
            g_rendTick = g_tick;   // renderer path available (also if this frame was already drawn through the fallback)
        }
        if (r == g_mainRend && g_drawnTick != g_tick) {
            if (!g_measured && g_measureStart) ++g_endFrames;
            Vec3 sc{0, 0, 0};
            if (SafeRPrepare(r, &sc)) {
                SizesOncePerSecond();
                g_rendScale = sc.x;
                if (!g_scaleLogged) {
                    g_scaleLogged = true;
                    char b[160];
                    std::snprintf(b, sizeof b, "Zeichnen: Haupt-Renderer %p, Zeichen-Massstab des Renderers %.3f x %.3f", r, sc.x, sc.y);
                    g_host->Log(b);
                }
                const Mat4 id0 = Identity();   // the renderer includes its drawing scale itself (measured: if not, doubled)
                SafeRXform(r, &id0);
                g_drawnTick = g_tick;
                g_rend = r;
                g_inDraw = true;
                g_host->CallSimple("void OnDraw()");
                g_inDraw = false;
                g_rend = nullptr;
                const Mat4 id = Identity();
                SafeRXform(r, &id);
                CountDraw(true);
            }
        }
    }
    return o_endFrame(r, flag);
}

bool CanDraw(const char* what) {
    if (g_inDraw && (g_dc || g_rend)) return true;
    if (!g_outsideLogged && ScriptHost::s_current) {
        g_outsideLogged = true;
        ScriptHost::s_current->Log(std::string(what) + ": nur innerhalb von OnDraw() (weitere Meldungen unterdrueckt)");
    }
    return false;
}

// images: loaded 1 time for each name (they stay until the end of the game, as the images of the game)
struct Bitmap {   // CFFGFCBitmap
    void* vt;
    void* handle;   // IFFGUIBitmap*
};
struct Image {
    Bitmap* gfc = nullptr;   // fallback path
    // SFFGUIBitmap from eCGUIAdmin::LoadBitmapA (measured in the assembler code 2026-11-09): +0 texture number, +4 bCRect part, +0x18..+0x24 four floats,
    // +0x28 bCString name
    alignas(8) unsigned char info[0x60] = {};
    bool infoOk = false, strReady = false, gfcOnlyLogged = false;
    std::string name;
    DWORD nextTry = 0;   // next try for LoadBitmapA (only GFC loaded)
    unsigned tex() const { return *reinterpret_cast<const unsigned*>(info); }
    const Rect* src() const { return reinterpret_cast<const Rect*>(info + 4); }
};
std::map<std::string, int> g_imageIds;      // only images that loaded successfully
std::map<std::string, DWORD> g_imageRetry;  // names that failed: next try (the GUI data can be missing at first)
std::vector<Image*> g_images;   // index = image number - 1
bool SafeBmpCreate(Bitmap* b, const SStr& name) {
    __try {
        g_a.bmpCtor(b);
        return g_a.bmpCreate(b, &name) && b->handle;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeStrCtor(void* p) {
    __try {
        g_sb.strCtor(p);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool SafeLoadInfo(void* admin, const SStr& name, unsigned char* info) {
    __try {
        return g_a.loadBmp(admin, &name, info);
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
// get the texture and the part for the renderer path (eCGUIAdmin::LoadBitmapA); the bCString in the SFFGUIBitmap is made only 1 time
void TryInfo(Image* img, const SStr& name) {
    if (img->infoOk || !g_a.loadBmp) return;
    void* admin = SafeAdmin();
    if (!admin) return;
    if (!img->strReady) img->strReady = SafeStrCtor(img->info + 0x28);
    if (!img->strReady) return;
    const Rect* s = img->src();
    img->infoOk = SafeLoadInfo(admin, name, img->info) && s->right > s->left && s->bottom > s->top;
}
bool SafeDcBitmap(void* dc, Bitmap* b, const Rect* r, float angle) {
    __try {
        return g_a.dcBitmap(dc, b, r, 0, angle);
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
    // A failure (0, for example in main() while the GUI is not ready) is not kept forever: a new try maximum each 2 s.
    // The log shows only the first failure for each font.
    static std::map<std::string, DWORD> retry;
    const DWORD now = GetTickCount();
    auto rt = retry.find(key);
    const bool firstTry = rt == retry.end();
    if (!firstTry && int(now - rt->second) < 0) return 0;
    const unsigned f = SafeCreateFont(face.c_str(), height, bold, italic);
    if (f) {
        g_fonts[key] = f;
        retry.erase(key);
    } else
        retry[key] = now + 2000u;
    if (ScriptHost::s_current && (f || firstTry)) {
        char b[200];
        std::snprintf(b, sizeof b, "Zeichnen: Schrift '%s' %d%s%s -> Handle %u", face.c_str(), height, bold ? " fett" : "", italic ? " kursiv" : "", f);
        ScriptHost::s_current->Log(b);
    }
    return int(f);
}
bool DrawOne(int font, int x, int y, const SStr& text, unsigned argb) {
    Rect r{x, y, x + 4000, y + 1000};
    if (g_rend) return SafeRText(g_rend, unsigned(font), text.c_str(), &r, kNoClip, argb);
    FColor c;
    if (!g_a.dcText || !g_a.colorCtor || !SafeColor(&c, argb)) return false;
    return SafeDcText(g_dc, unsigned(font), text.c_str(), &r, kNoClip, &c);
}
bool DrawTextScript(int font, int x, int y, const SStr& text, unsigned argb, bool shadow) {
    if (!CanDraw("DrawText")) return false;
    if (shadow) DrawOne(font, x + 1, y + 1, text, argb & 0xFF000000u);
    return DrawOne(font, x, y, text, argb);
}
int TextWidthScript(int font, const SStr& text) {
    if (!CanDraw("TextWidth")) return 0;
    Rect r{0, 0, 4000, 1000};
    if (g_rend)
        SafeRCalc(g_rend, unsigned(font), text.c_str(), &r, kNoClip);
    else if (g_a.dcCalc)
        SafeDcCalc(g_dc, unsigned(font), text.c_str(), &r, kNoClip);
    return r.right - r.left;
}
// Load an image (cached for each name): number > 0, 0 = cannot be loaded (yet). Only in the game thread. A name that failed is tried again maximum each 2 s
// (in main(), the GUI data is frequently still missing). The log shows only the first failure.
int LoadImageScript(const SStr& name) {
    auto it = g_imageIds.find(name.c_str());
    if (it != g_imageIds.end()) return it->second;
    if (g_gameThread && GetCurrentThreadId() != g_gameThread) return 0;
    const DWORD now = GetTickCount();
    auto rt = g_imageRetry.find(name.c_str());
    const bool firstTry = rt == g_imageRetry.end();
    if (!firstTry && int(now - rt->second) < 0) return 0;
    Image* img = new Image;
    img->name = name.c_str();
    TryInfo(img, name);
    if (g_a.bmpCtor && g_a.bmpCreate) {
        img->gfc = new Bitmap{nullptr, nullptr};
        if (!SafeBmpCreate(img->gfc, name)) {
            delete img->gfc;
            img->gfc = nullptr;
        }
    }
    int id = 0;
    if (img->infoOk || img->gfc) {
        g_images.push_back(img);
        id = int(g_images.size());
        g_imageIds[img->name] = id;
        g_imageRetry.erase(img->name);
    } else {
        g_imageRetry[img->name] = now + 2000u;
        delete img;   // the bCString in the buffer stays (a few bytes for each failure)
        img = nullptr;
    }
    if (ScriptHost::s_current && (id || firstTry)) {
        char b[360];
        if (img && img->infoOk) {
            const Rect* s = img->src();
            std::snprintf(b, sizeof b, "Zeichnen: Bild '%.200s' -> %d (Textur %u, Ausschnitt %d,%d-%d,%d%s)", name.c_str(), id, img->tex(), s->left, s->top,
                          s->right, s->bottom, img->gfc ? ", auch GFC" : "");
        } else {
            std::snprintf(b, sizeof b, "Zeichnen: Bild '%.200s' -> %d%s", name.c_str(), id,
                          id ? " (nur GFC: ueber dem HUD erst, wenn LoadBitmapA es findet)" : " (nicht geladen, neuer Versuch spaeter)");
        }
        ScriptHost::s_current->Log(b);
    }
    return id;
}
bool DrawImageScript(int image, int x, int y, int w, int h, float angle) {
    if (!CanDraw("DrawImage") || image <= 0 || image > int(g_images.size()) || w <= 0 || h <= 0) return false;
    Image* img = g_images[image - 1];
    if (g_rend) {
        if (!img->infoOk && int(GetTickCount() - img->nextTry) >= 0) {   // loaded only through GFC: get the renderer data again maximum each 2 s
            img->nextTry = GetTickCount() + 2000u;
            SStr n(img->name.c_str());
            TryInfo(img, n);
            if (!img->infoOk && !img->gfcOnlyLogged && ScriptHost::s_current) {
                img->gfcOnlyLogged = true;
                ScriptHost::s_current->Log("Zeichnen: Bild '" + img->name + "' hat keine Renderer-Daten (LoadBitmapA) und wird ueber dem HUD nicht gezeichnet");
            }
        }
        if (!img->infoOk || !g_a.rSprite) return false;
        const Rect* s = img->src();
        // scale the part to w x h, turn it around the center, put it at (x, y), then GUI area -> window
        Mat4 m = Scale(float(w) / float(s->right - s->left), float(h) / float(s->bottom - s->top));
        m = Mul(m, Translate(-0.5f * w, -0.5f * h));
        if (angle != 0.0f) m = Mul(m, Rotate(angle));
        m = Mul(m, Translate(float(x) + 0.5f * w, float(y) + 0.5f * h));
        const bool ok = SafeRSprite(g_rend, img->tex(), s, &m);
        const Mat4 back = Identity();   // DrawSprite sets the transformation; reset it for the text that follows
        SafeRXform(g_rend, &back);
        return ok;
    }
    if (!img->gfc || !g_a.dcBitmap) return false;
    Rect r{x, y, x + w, y + h};
    return SafeDcBitmap(g_dc, img->gfc, &r, angle);
}
int ScreenWidth() { return g_screenW; }
int ScreenHeight() { return g_screenH; }

// world point -> drawing area; false if the point is behind the camera or if there is no camera
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
    G3ASM_REG(e->RegisterGlobalFunction("int CreateFont(const bCString &in face, int height, bool bold = false, bool italic = false)",
                                      asFUNCTION(CreateFontScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction(
        "bool DrawText(int font, int x, int y, const bCString &in text, uint argb = 0xFFFFFFFF, bool shadow = true)", asFUNCTION(DrawTextScript),
        asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int TextWidth(int font, const bCString &in text)", asFUNCTION(TextWidthScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int LoadImage(const bCString &in name)", asFUNCTION(LoadImageScript), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool DrawImage(int image, int x, int y, int w, int h, float angle = 0)", asFUNCTION(DrawImageScript),
                                      asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetScreenWidth()", asFUNCTION(ScreenWidth), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("int GetScreenHeight()", asFUNCTION(ScreenHeight), asCALL_CDECL));
    G3ASM_REG(e->RegisterGlobalFunction("bool WorldToScreen(const bCVector &in pos, float &out x, float &out y)", asFUNCTION(WorldToScreenScript),
                                      asCALL_CDECL));
    return ok;
}

// 1 time for each game frame from HProcess (game thread): frame counter for "maximum 1 OnDraw for each frame"
void DrawFrameTick() {
    if (!g_gameThread) g_gameThread = GetCurrentThreadId();
    ++g_tick;
}

int InstallDrawHooks(ScriptHost& host) {
    g_host = &host;
    Resolve();
    const bool ok = HookExport(GetModuleHandleA("GFC.dll"), "?OnPaint@CFFGFCWnd@@UAEXXZ", reinterpret_cast<void*>(&HPaint),
                               reinterpret_cast<void**>(&o_paint), host);
    const bool okEnd = RendererReady() && HookExport(GetModuleHandleA("Engine.dll"), "?EndFrame@eCGUIRenderer@@UAE_N_N@Z", reinterpret_cast<void*>(&HEndFrame),
                                                     reinterpret_cast<void**>(&o_endFrame), host);
    char b[320];
    std::snprintf(b, sizeof b,
                  "Zeichnen: OnPaint-Hook (GFC.dll) %s, Bildende des Renderers %s; Text ja/%s, Schrift %s, Farbe %s, Kamera %s, Bild %s/%s",
                  ok ? "aktiv" : "FEHLT", okEnd ? "aktiv (ueber dem HUD)" : "FEHLT", g_a.dcText ? "ja" : "nein", g_a.createFont ? "ja" : "nein",
                  g_a.colorCtor ? "ja" : "nein", g_a.w2s && g_a.getCam ? "ja" : "nein", g_a.loadBmp && g_a.rSprite ? "ja" : "nein",
                  g_a.bmpCreate && g_a.dcBitmap ? "ja" : "nein");
    host.Log(b);
    return ok ? 1 : 0;
}

}   // namespace g3asm
