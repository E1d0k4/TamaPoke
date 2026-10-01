// TamaDash.h - verstecktes Endless-Runner-Easter-Egg fuer TamaPoke
// Vollstaendig gekapselt: keine Abhaengigkeit zur Pet-/Spiel-Logik.

#pragma once

#include <Arduino.h>

// Easter-Egg-Erkennung
void tamaDashResetEasterEgg();
bool tamaDashHandleSunTap(int16_t x, int16_t y);

// Spiel
// Die bestehende TamaPoke-Integration fragt diese Funktion als Aktiv-Status ab.
bool tamaDashOpen();
bool tamaDashTap(int16_t x, int16_t y);
void tamaDashRender();
