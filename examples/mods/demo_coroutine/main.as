// Example mod: coroutines (from version 47). F10 starts a countdown on the screen, F11 stops it.
// A second coroutine writes the coroutine clock and the real time to the log every 10 seconds. The log shows if Wait() pauses with the game
// (menu, inventory, dialog).
// Installation: copy the folder to <Gothic3>\g3asm\mods\demo_coroutine and add "+demo_coroutine" to g3asm\mods\load_order.txt.

int countdown = 0;   // ID of the running countdown, 0 = none

void Countdown(dictionary@ args) {
    int from;
    if (!args.get("from", from)) from = 3;
    for (int i = from; i > 0; i--) {
        PrintScreen("" + i, 1, 255, 220, 120);
        Wait(1.0f);
    }
    PrintScreen("Los!", 2, 140, 255, 140);
    print("Countdown fertig");
    countdown = 0;
}

void Clock() {
    uint start = GetTimeStamp();
    float sum = 0;
    while (true) {
        Wait(10.0f);
        sum += 10.0f;
        print("Koroutinen-Uhr " + sum + " s, echte Zeit " + ((GetTimeStamp() - start) / 1000.0f) + " s");
    }
}

void main() {
    StartCoroutine(Clock);
    print("demo_coroutine bereit: F10 = Countdown, F11 = Abbruch");
}

void OnKey(int vk, bool down) {
    if (!down) return;
    if (vk == KEY_F10 && !IsCoroutineRunning(countdown)) {
        dictionary args = {{"from", 5}};
        countdown = StartCoroutine(Countdown, args);
        print("Countdown gestartet, ID " + countdown);
    } else if (vk == KEY_F11 && IsCoroutineRunning(countdown)) {
        StopCoroutine(countdown);
        PrintScreen("Abgebrochen", 2, 255, 120, 120);
        print("Countdown abgebrochen");
        countdown = 0;
    }
}
