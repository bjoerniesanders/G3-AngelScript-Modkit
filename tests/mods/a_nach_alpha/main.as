void main() {
    check(IsModActive("alpha") && IsModActive("ZETA") && GetModVersion("alpha") == "1.2.0", "Abhaengigkeit: alpha und zeta aktiv, Version gelesen");
    check(GetModVersion("a_nach_alpha") == "0.1" && GetModVersion("zeta").IsEmpty(), "Abhaengigkeit: eigene Version, zeta ohne Versionsangabe");
    check(!IsModActive("aus") && !IsModActive("braucht_fehlt") && !IsModActive("gibts_nicht"), "Abhaengigkeit: gesperrte Mods sind nicht aktiv");
}
