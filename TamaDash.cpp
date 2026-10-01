#include "TamaDash.h"

#include <Arduino.h>
#include <Preferences.h>
#include "Arduino_GFX_Library.h"
#include "audio.h"

extern Arduino_Canvas *gfx;

namespace {
constexpr int16_t CX = 233;
constexpr int16_t CY = 233;
constexpr int16_t GROUND_Y = 380;
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

float gPlayerY = GROUND_Y;
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

void resetRun() {
  gGameOver = false;
  gScore = 0;
  gPlayerY = GROUND_Y;
  gPlayerVY = 0;
  gObstacleX = 520 + random(0, 90);
  gRunStart = millis();
  gLastStep = gRunStart;
}

bool onGround() {
  return gPlayerY >= GROUND_Y;
}

void jump() {
  if (gGameOver) {
    resetRun();
    return;
  }
  if (onGround()) {
    gPlayerVY = -13.0f;
    sfxPlay(SFX_PLAY);
  }
}

bool hitObstacle() {
  const float playerLeft = PLAYER_X - PLAYER_W / 2.0f;
  const float playerRight = PLAYER_X + PLAYER_W / 2.0f;
  const float playerTop = gPlayerY - PLAYER_H;
  const float playerBottom = gPlayerY;

  const float obstacleLeft = gObstacleX - 15.0f;
  const float obstacleRight = gObstacleX + 15.0f;
  const float obstacleTop = GROUND_Y - 54.0f;

  return playerRight > obstacleLeft && playerLeft < obstacleRight &&
         playerBottom > obstacleTop && playerTop < GROUND_Y;
}

void step() {
  uint32_t now = millis();
  float dt = gLastStep ? (now - gLastStep) / 16.0f : 1.0f;
  if (dt > 3.0f) dt = 3.0f;
  if (dt < 0.0f) dt = 0.0f;
  gLastStep = now;

  gPlayerVY += 0.72f * dt;
  gPlayerY += gPlayerVY * dt;
  if (gPlayerY >= GROUND_Y) {
    gPlayerY = GROUND_Y;
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
  uint16_t obstacle = rgb565(80, 145, 92);
  int x = (int)gObstacleX;
  int top = GROUND_Y - 54;
  gfx->fillRoundRect(x - 15, top, 30, 54, 8, obstacle);
  gfx->drawRoundRect(x - 15, top, 30, 54, 8, ink);
  gfx->fillRect(x - 7, top - 10, 6, 12, obstacle);
  gfx->fillRect(x + 1, top - 5, 6, 7, obstacle);
}

void drawScene() {
  uint16_t sky = rgb565(220, 238, 230);
  uint16_t soil = rgb565(126, 192, 127);
  uint16_t ink = rgb565(24, 28, 38);

  gfx->fillCircle(CX, CY, 231, sky);
  gfx->fillRect(0, GROUND_Y, 466, 466 - GROUND_Y, soil);
  gfx->fillRect(0, GROUND_Y - 2, 466, 4, ink);

  // Kleine bewegte Wolken fuer ein lebendigeres, aber bewusst schlichtes Feld.
  int cloud = (int)((millis() / 45) % 560) - 60;
  gfx->fillCircle(cloud, 105, 13, 0xFFFF);
  gfx->fillCircle(cloud + 18, 108, 10, 0xFFFF);
  gfx->fillCircle(cloud - 15, 110, 9, 0xFFFF);
}

void drawScore() {
  uint16_t ink = rgb565(24, 28, 38);
  char score[20];
  snprintf(score, sizeof(score), "SCORE: %04u", gScore);
  gfx->setTextColor(ink);
  gfx->setTextSize(2);
  gfx->setCursor(52, 54);
  gfx->print(score);
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
