#include "TamaDash.h"

#include <Arduino.h>
#include <Preferences.h>
#include "Arduino_GFX_Library.h"
#include "audio.h"

extern Arduino_Canvas *gfx;

namespace {
constexpr int16_t CX = 233;
constexpr int16_t CY = 233;
constexpr int16_t GROUND_CENTER_Y = 350;
constexpr int16_t GROUND_CURVE = 1600;
constexpr int16_t PLAYER_X = 112;
constexpr int16_t PLAYER_W = 34;
constexpr int16_t PLAYER_H = 46;
constexpr int16_t BACK_X0 = 188;
constexpr int16_t BACK_X1 = 278;
constexpr int16_t BACK_Y1 = 76;
constexpr uint32_t EASTER_TAP_GAP_MS = 1500;

bool gOpen = false;
bool gGameOver = false;
uint8_t gEggTaps = 0;
uint32_t gLastEggTap = 0;

float gPlayerY = 0;
float gPlayerVY = 0;
float gObstacleX = 520;
uint16_t gScore = 0;
uint16_t gBest = 0;
uint32_t gRunStart = 0;
uint32_t gLastStep = 0;

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)((((r >> 3) << 11)) | (((g >> 2) << 5)) | (b >> 3));
}

void loadBest() {
  Preferences p;
  p.begin("tamadash", true);
  gBest = p.getUShort("best", 0);
  p.end();
}

void saveBest() {
  Preferences p;
  p.begin("tamadash", false);
  p.putUShort("best", gBest);
  p.end();
}

int16_t groundYAt(int16_t x) {
  const float dx = (float)x - CX;
  return (int16_t)(GROUND_CENTER_Y + (dx * dx) / GROUND_CURVE);
}

void resetRun() {
  gGameOver = false;
  gScore = 0;
  gPlayerY = groundYAt(PLAYER_X);
  gPlayerVY = 0;
  gObstacleX = 520 + random(0, 90);
  gRunStart = millis();
  gLastStep = gRunStart;
}

bool onGround() {
  return gPlayerY >= groundYAt(PLAYER_X);
}

void jump() {
  if (gGameOver) {
    resetRun();
    return;
  }

  if (onGround()) {
    gPlayerVY = -13.0f;
    sfxPlay(SFX_PLAY);
    return;
  }

  // Zweiter Tap waehrend des Sprungs: sofortiger Fast-Fall.
  // Dadurch fuehlt sich die Steuerung direkter und arcade-artiger an.
  gPlayerVY = 11.0f;
}

bool hitObstacle() {
  const float playerLeft = PLAYER_X - PLAYER_W / 2.0f;
  const float playerRight = PLAYER_X + PLAYER_W / 2.0f;
  const float playerTop = gPlayerY - PLAYER_H;
  const float playerBottom = gPlayerY;

  const float obstacleLeft = gObstacleX - 15.0f;
  const float obstacleRight = gObstacleX + 15.0f;
  const float obstacleGround = groundYAt((int16_t)gObstacleX);
  const float obstacleTop = obstacleGround - 54.0f;

  return playerRight > obstacleLeft && playerLeft < obstacleRight &&
         playerBottom > obstacleTop && playerTop < obstacleGround;
}

void step() {
  uint32_t now = millis();
  float dt = gLastStep ? (now - gLastStep) / 16.0f : 1.0f;
  if (dt > 3.0f) dt = 3.0f;
  if (dt < 0.0f) dt = 0.0f;
  gLastStep = now;

  gPlayerVY += 0.72f * dt;
  gPlayerY += gPlayerVY * dt;
  const float playerGround = groundYAt(PLAYER_X);
  if (gPlayerY >= playerGround) {
    gPlayerY = playerGround;
    gPlayerVY = 0;
  }

  float speed = 5.0f + gScore * 0.035f;
  if (speed > 9.0f) speed = 9.0f;
  gObstacleX -= speed * dt;

  gScore = (uint16_t)((now - gRunStart) / 100);
  if (gObstacleX < -40) {
    gObstacleX = 500 + random(0, 130);
  }

  if (hitObstacle()) {
    gGameOver = true;
    if (gScore > gBest) {
      gBest = gScore;
      saveBest();
      sfxPlay(SFX_MEDAL);
    } else {
      sfxPlay(SFX_LEVEL);
    }
  }
}

void drawBackArrow() {
  uint16_t ink = rgb565(24, 28, 38);
  gfx->drawLine(CX + 13, 34, CX - 12, 34, ink);
  gfx->drawLine(CX - 12, 34, CX - 2, 24, ink);
  gfx->drawLine(CX - 12, 34, CX - 2, 44, ink);
}

void drawPlayer() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t fur = rgb565(204, 132, 72);
  uint16_t belly = rgb565(244, 222, 184);
  uint16_t ear = rgb565(232, 151, 92);
  int x = PLAYER_X;
  int bottom = (int)gPlayerY;

  // Kleiner Fuchs schaut nach links, in Laufrichtung.
  gfx->fillRoundRect(x - 17, bottom - 37, 34, 31, 10, fur);
  gfx->drawRoundRect(x - 17, bottom - 37, 34, 31, 10, ink);

  gfx->fillTriangle(x - 14, bottom - 35, x - 8, bottom - 48,
                    x - 1, bottom - 36, fur);
  gfx->fillTriangle(x + 1, bottom - 36, x + 8, bottom - 48,
                    x + 14, bottom - 35, fur);
  gfx->fillTriangle(x - 9, bottom - 37, x - 8, bottom - 43,
                    x - 3, bottom - 37, ear);
  gfx->fillTriangle(x + 3, bottom - 37, x + 8, bottom - 43,
                    x + 9, bottom - 37, ear);

  gfx->fillRoundRect(x - 8, bottom - 17, 16, 10, 5, belly);
  gfx->fillCircle(x - 7, bottom - 26, 3, ink);
  gfx->fillCircle(x + 7, bottom - 26, 3, ink);
  gfx->fillCircle(x - 10, bottom - 18, 2, ink);

  int phase = ((millis() / 110) & 1) ? 3 : -3;
  gfx->fillRoundRect(x - 11 + phase, bottom - 8, 7, 8, 3, ink);
  gfx->fillRoundRect(x + 4 - phase, bottom - 8, 7, 8, 3, ink);

  // Schwanz jetzt hinten links, damit er nicht wie ein Schild aussieht.
  gfx->fillCircle(x - 19, bottom - 20, 11, fur);
  gfx->fillCircle(x - 24, bottom - 20, 7, belly);
  gfx->drawCircle(x - 19, bottom - 20, 11, ink);
}

void drawObstacle() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t trunk = rgb565(117, 82, 54);
  uint16_t leaf = rgb565(72, 145, 82);
  uint16_t berry = rgb565(191, 62, 76);

  int x = (int)gObstacleX;
  int base = groundYAt((int16_t)gObstacleX);

  // Beerenbusch: niedrig und breit, damit der Spieler darueber springen kann.
  gfx->fillCircle(x - 15, base - 13, 12, leaf);
  gfx->fillCircle(x, base - 20, 16, leaf);
  gfx->fillCircle(x + 15, base - 12, 12, leaf);
  gfx->fillRoundRect(x - 19, base - 8, 38, 8, 4, trunk);

  gfx->fillCircle(x - 8, base - 19, 3, berry);
  gfx->fillCircle(x + 6, base - 25, 3, berry);
  gfx->fillCircle(x + 15, base - 12, 3, berry);
  gfx->drawCircle(x - 15, base - 13, 12, ink);
  gfx->drawCircle(x, base - 20, 16, ink);
  gfx->drawCircle(x + 15, base - 12, 12, ink);
  gfx->drawRoundRect(x - 19, base - 8, 38, 8, 4, ink);
}

void drawScene() {
  uint16_t sky = rgb565(220, 238, 230);
  uint16_t soil = rgb565(126, 192, 127);
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t tree = rgb565(74, 130, 76);
  uint16_t trunk = rgb565(117, 82, 54);

  gfx->fillCircle(CX, CY, 231, sky);

  // Sanftere Planeten-Kruemmung: etwa halb so stark wie zuvor.
  for (int x = 0; x < 466; ++x) {
    const int y = groundYAt(x);
    if (y < 466) {
      gfx->drawFastVLine(x, y, 466 - y, soil);
      gfx->drawFastHLine(x, y, 1, ink);
    }
  }

  // Waldkulisse: die Baeume folgen der gekruemmten Welt.
  // Ihre Wurzeln sitzen auf dem Boden und die Stämme neigen sich
  // leicht nach aussen, sodass die Kulisse die Rundung sichtbar aufnimmt.
  for (int x = 20; x < 466; x += 92) {
    int sway = ((millis() / 700 + x) % 8) - 4;
    int base = groundYAt((int16_t)x);
    int lean = (x - CX) / 14;
    int trunkTopX = x + sway + lean;
    int trunkTopY = base - 82;

    gfx->drawLine(x + sway - 5, base, trunkTopX - 5, trunkTopY, trunk);
    gfx->drawLine(x + sway + 5, base, trunkTopX + 5, trunkTopY, trunk);

    gfx->fillCircle(trunkTopX, trunkTopY - 8, 24, tree);
    gfx->fillCircle(trunkTopX - 18, trunkTopY + 5, 18, tree);
    gfx->fillCircle(trunkTopX + 18, trunkTopY + 5, 18, tree);
  }

  // Kleine bewegte Wolken.
  int cloud = (int)((millis() / 45) % 560) - 60;
  gfx->fillCircle(cloud, 82, 13, 0xFFFF);
  gfx->fillCircle(cloud + 18, 85, 10, 0xFFFF);
  gfx->fillCircle(cloud - 15, 87, 9, 0xFFFF);
}

void drawScore() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t panel = rgb565(248, 248, 238);

  char score[20];
  char best[20];
  // Kuerzere Labels: Textgroesse 2 bleibt gut lesbar und passt sauber
  // in den vorhandenen kompakten Hintergrund.
  snprintf(score, sizeof(score), "SCORE %04u", gScore);
  snprintf(best, sizeof(best), "BEST  %04u", gBest);

  gfx->fillRoundRect(164, 50, 138, 70, 12, panel);
  gfx->drawRoundRect(164, 50, 138, 70, 12, ink);

  gfx->setTextColor(ink);
  gfx->setTextSize(2);
  gfx->setCursor(177, 65);
  gfx->print(score);
  gfx->setCursor(183, 95);
  gfx->print(best);
}

void drawGameOver() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t accent = rgb565(215, 70, 70);

  gfx->fillRoundRect(64, 135, 338, 190, 20, 0xFFFF);
  gfx->drawRoundRect(64, 135, 338, 190, 20, ink);

  gfx->setTextColor(accent);
  gfx->setTextSize(4);
  gfx->setCursor(112, 160);
  gfx->print("GAME OVER");

  char score[20];
  char best[20];
  snprintf(score, sizeof(score), "SCORE: %04u", gScore);
  snprintf(best, sizeof(best), "BEST:  %04u", gBest);

  gfx->setTextColor(ink);
  gfx->setTextSize(2);
  gfx->setCursor(139, 225);
  gfx->print(score);
  gfx->setCursor(139, 255);
  gfx->print(best);

  gfx->setCursor(125, 292);
  gfx->print("TOUCH = RUN");
}

} // namespace

bool tamaDashOpen() {
  return gOpen;
}

void tamaDashResetEasterEgg() {
  gEggTaps = 0;
  gLastEggTap = 0;
}

bool tamaDashHandleSunTap(int16_t x, int16_t y) {
  // Unsichtbare Trefferzone um das bestehende Sonnen-Symbol.
  const int dx = x - 72;
  const int dy = y - 226;
  if (dx * dx + dy * dy > 34 * 34) return false;

  uint32_t now = millis();
  if (gLastEggTap && now - gLastEggTap > EASTER_TAP_GAP_MS) {
    gEggTaps = 0;
  }

  gEggTaps++;
  gLastEggTap = now;

  if (gEggTaps >= 5) {
    gEggTaps = 0;
    gLastEggTap = 0;
    gOpen = true;
    loadBest();
    resetRun();
  }
  return true;
}

bool tamaDashTap(int16_t x, int16_t y) {
  if (!gOpen) return false;

  // Sichtbarer Pfeil klein, Touch-Zone bewusst groesser.
  if (x >= BACK_X0 && x <= BACK_X1 && y <= BACK_Y1) {
    gOpen = false;
    gGameOver = false;
    tamaDashResetEasterEgg();
    return true;
  }

  jump();
  return false;
}

void tamaDashRender() {
  if (!gOpen) return;

  drawScene();
  drawBackArrow();

  if (!gGameOver) {
    step();
    drawObstacle();
    drawPlayer();
    drawScore();
  } else {
    drawGameOver();
  }

  gfx->flush();
}
