int ovr = 0;
bool Override_Test(uint v) { ovr++; return Super(); }
int keys = 0, frames = 0;
void main() { print("main"); }
void OnKey(int vk, bool down) { keys++; }
void OnFrame(float dt) { frames++; }
void OnShutdown() { check(ovr == 1, "alpha: Override-Kette genau einmal"); check(keys == 2 && frames == 1, "alpha: Tasten/Frames"); }
