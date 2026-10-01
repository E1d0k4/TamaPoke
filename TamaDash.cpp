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
constexpr int16_t GROUND_CURVE = 800;
constexpr int16_t PLAYER_X = 112;
constexpr int16_t PLAYER_W = 38;
constexpr int16_t PLAYER_H = 54;
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
  uint16_t body = rgb565(245, 245, 245);
  int x = PLAYER_X;
  int bottom = (int)gPlayerY;

  // Ein bewusst schlichtes eigenes Tama-Dash-Sprite:
  // Dadurch bleibt das Easter Egg komplett unabhaengig von der normalen Pet-Logik.
  gfx->fillRoundRect(x - 19, bottom - 42, 38, 38, 10, body);
  gfx->drawRoundRect(x - 19, bottom - 42, 38, 38, 10, ink);
  gfx->fillCircle(x - 7, bottom - 27, 3, ink);
  gfx->fillCircle(x + 7, bottom - 27, 3, ink);
  gfx->fillRect(x - 8, bottom - 16, 16, 3, ink);

  // Beine geben dem Lauf eine kleine, sichtbare Animation.
  int phase = ((millis() / 110) & 1) ? 4 : -4;
  gfx->fillRoundRect(x - 13 + phase, bottom - 6, 8, 9, 3, ink);
  gfx->fillRoundRect(x + 5 - phase, bottom - 6, 8, 9, 3, ink);
}

void drawObstacle() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t trunk = rgb565(154, 111, 72);
  uint16_t leaf = rgb565(78, 156, 92);

  int x = (int)gObstacleX;
  int base = groundYAt((int16_t)gObstacleX);

  // Kleiner Kaktus / Busch als klarere, weichere Pixel-Form.
  gfx->fillRoundRect(x - 11, base - 48, 22, 48, 8, leaf);
  gfx->fillRoundRect(x - 22, base - 38, 12, 24, 6, leaf);
  gfx->fillRoundRect(x + 10, base - 31, 12, 21, 6, leaf);
  gfx->fillRoundRect(x - 3, base - 53, 6, 8, 3, leaf);

  gfx->drawRoundRect(x - 11, base - 48, 22, 48, 8, ink);
  gfx->drawRoundRect(x - 22, base - 38, 12, 24, 6, ink);
  gfx->drawRoundRect(x + 10, base - 31, 12, 21, 6, ink);

  // Ein kleiner Stamm/Stein als zweite Silhouette, damit es weniger nach einem Block aussieht.
  gfx->fillRoundRect(x - 16, base - 8, 32, 8, 4, trunk);
}

void drawScene() {
  uint16_t sky = rgb565(220, 238, 230);
  uint16_t soil = rgb565(126, 192, 127);
  uint16_t ink = rgb565(24, 28, 38);

  gfx->fillCircle(CX, CY, 231, sky);

  // Leicht gekruemmter Horizont: Tama laeuft sichtbar auf einem kleinen
  // Planeten statt auf einer flachen Plattform.
  for (int x = 0; x < 466; ++x) {
    const int y = groundYAt(x);
    if (y < 466) {
      gfx->drawFastVLine(x, y, 466 - y, soil);
      gfx->drawFastHLine(x, y, 1, ink);
    }
  }

  // Kleine bewegte Wolken fuer ein lebendigeres, aber bewusst schlichtes Feld.
  int cloud = (int)((millis() / 45) % 560) - 60;
  gfx->fillCircle(cloud, 105, 13, 0xFFFF);
  gfx->fillCircle(cloud + 18, 108, 10, 0xFFFF);
  gfx->fillCircle(cloud - 15, 110, 9, 0xFFFF);
}

void drawScore() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t panel = rgb565(248, 248, 238);

  char score[20];
  char best[20];
  snprintf(score, sizeof(score), "SCORE: %04u", gScore);
  snprintf(best, sizeof(best), "BEST:  %04u", gBest);

  // Kompaktes HUD oben mittig. Die beiden Werte stehen untereinander,
  // ohne den eigentlichen Spielbereich mit einer grossen Leiste zu verdecken.
  gfx->fillRoundRect(174, 54, 118, 62, 12, panel);
  gfx->drawRoundRect(174, 54, 118, 62, 12, ink);

  gfx->setTextColor(ink);
  gfx->setTextSize(1);
  gfx->setCursor(200, 67);
  gfx->print(score);
  gfx->setCursor(200, 91);
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
