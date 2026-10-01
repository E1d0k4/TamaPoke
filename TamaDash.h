#pragma once
#include <stdint.h>

// Verstecktes, vom Hauptspiel getrenntes Mini-Spiel.
bool tamaDashOpen();
void tamaDashResetEasterEgg();

// Wird nur auf der bestehenden Helligkeitsseite aufgerufen.
// Liefert true, wenn der Tap vom Easter Egg verarbeitet wurde.
bool tamaDashHandleSunTap(int16_t x, int16_t y);

// true = Tama Dash wurde verlassen; der Aufrufer kann zur Startseite zurueckkehren.
bool tamaDashTap(int16_t x, int16_t y);

void tamaDashRender();
