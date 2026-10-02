// Vegg.h - verstecktes Eevee-Jump-Easter-Egg fuer TamaPoke
// Vollstaendig gekapselt: keine Abhaengigkeit zur Pet-/Spiel-Logik.

#pragma once

#include <Arduino.h>

// --- Easter-Egg-Erkennung ---------------------------------------------------

void veggResetEasterEgg();                       // Zaehler zuruecksetzen
bool veggHandleVersionTap(int16_t x, int16_t y);  // true = der versteckte Versions-Touch wurde ausgeloest

// --- Spiel ------------------------------------------------------------------

void veggOpen();                                 // Spiel starten
void veggTap(int16_t x, int16_t y);              // EIN Aufruf pro neuem Touch-Down
bool veggRender();                               // false = Spiel beendet -> zur alten Seite zurueck
bool veggActive();                                // true = Tama Dash laeuft
uint32_t veggBestScore();
void veggSetBestScore(uint32_t value);
