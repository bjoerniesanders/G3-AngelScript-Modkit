int ovr = 0;
bool Override_Test(uint v) { ovr++; return Super(); }
int frames = 0;
void main() { print("main"); }
void OnFrame(float dt) { frames++; }
void OnShutdown() { check(ovr == 1, "zeta: Override-Kette genau einmal"); check(frames == 1, "zeta: Frame"); }
