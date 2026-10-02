// TamaDash.h - verstecktes Eevee-Jump-Easter-Egg fuer TamaPoke
// Vollstaendig gekapselt: keine Abhaengigkeit zur Pet-/Spiel-Logik.

#pragma once

#include <Arduino.h>

// --- Easter-Egg-Erkennung ---------------------------------------------------

void tamaDashResetEasterEgg();                       // Zaehler zuruecksetzen
bool tamaDashHandleInfoTap(int16_t x, int16_t y);     // true = INFO-Touch wurde verarbeitet

// --- Spiel ------------------------------------------------------------------

void tamaDashOpen();                                 // Spiel starten
void tamaDashTap(int16_t x, int16_t y);              // EIN Aufruf pro neuem Touch-Down
bool tamaDashRender();                               // false = Spiel beendet -> zur alten Seite zurueck
bool tamaDashActive();                                // true = Tama Dash laeuft
uint32_t tamaDashBestScore();
void tamaDashSetBestScore(uint32_t value);
