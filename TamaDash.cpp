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
  uint16_t furDark = rgb565(170, 98, 52);
  uint16_t cream = rgb565(248, 222, 177);
  uint16_t ear = rgb565(235, 151, 91);
  int x = PLAYER_X;
  int bottom = (int)gPlayerY;

  // Kleiner Fuchs mit klarer Schnauze, Ohren und buschigem Schwanz.
  // Er laeuft nach rechts; der Schwanz sitzt deutlich dahinter links.
  gfx->fillCircle(x - 18, bottom - 19, 12, fur);
  gfx->fillCircle(x - 24, bottom - 19, 7, cream);
  gfx->drawCircle(x - 18, bottom - 19, 12, ink);

  gfx->fillRoundRect(x - 11, bottom - 34, 30, 27, 10, fur);
  gfx->drawRoundRect(x - 11, bottom - 34, 30, 27, 10, ink);

  // Spitzere Fuchsohren.
  gfx->fillTriangle(x - 8, bottom - 31, x - 5, bottom - 48,
                    x + 3, bottom - 32, fur);
  gfx->fillTriangle(x + 6, bottom - 31, x + 13, bottom - 48,
                    x + 18, bottom - 29, fur);
  gfx->fillTriangle(x - 5, bottom - 34, x - 5, bottom - 42,
                    x + 0, bottom - 34, ear);
  gfx->fillTriangle(x + 9, bottom - 34, x + 13, bottom - 42,
                    x + 16, bottom - 31, ear);

  // Weisse/creme Schnauze nach rechts + schwarze Nase.
  gfx->fillCircle(x + 14, bottom - 20, 8, cream);
  gfx->fillCircle(x + 20, bottom - 20, 3, ink);
  gfx->fillCircle(x + 7, bottom - 27, 2, ink);
  gfx->drawFastHLine(x + 11, bottom - 16, 7, furDark);

  // Heller Bauch.
  gfx->fillRoundRect(x - 5, bottom - 15, 14, 10, 5, cream);

  // Laufanimation.
  int phase = ((millis() / 110) & 1) ? 3 : -3;
  gfx->fillRoundRect(x - 9 + phase, bottom - 8, 7, 8, 3, furDark);
  gfx->fillRoundRect(x + 5 - phase, bottom - 8, 7, 8, 3, furDark);
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
  uint16_t sky = rgb565(211, 231, 220);
  uint16_t distant = rgb565(116, 157, 112);
  uint16_t midTree = rgb565(76, 126, 74);
  uint16_t tree = rgb565(54, 105, 61);
  uint16_t trunk = rgb565(112, 77, 49);
  uint16_t undergrowth = rgb565(71, 130, 71);
  uint16_t soil = rgb565(126, 192, 127);
  uint16_t ink = rgb565(24, 28, 38);

  gfx->fillCircle(CX, CY, 231, sky);

  // Sanfte Planeten-Kruemmung.
  for (int x = 0; x < 466; ++x) {
    const int y = groundYAt(x);
    if (y < 466) {
      gfx->drawFastVLine(x, y, 466 - y, soil);
      gfx->drawFastHLine(x, y, 1, ink);
    }
  }

  // Ferne Waldschicht: viele kleine, unregelmaessige Baumkronen statt einer Allee.
  for (int x = -10; x < 480; x += 47) {
    int wobble = ((x * 7) % 13) - 6;
    int base = groundYAt((int16_t)x);
    int top = 122 + ((x * 3) % 38);
    int tx = x + wobble;
    gfx->drawLine(tx, base - 2, tx + (x - CX) / 22, top + 28, distant);
    gfx->fillCircle(tx + (x - CX) / 22, top + 18, 18, distant);
    gfx->fillCircle(tx - 13 + (x % 9), top + 30, 13, distant);
    gfx->fillCircle(tx + 14, top + 31, 15, distant);
  }

  // Mittlere, groessere Baeume. Unterschiedliche Hoehen und Abstaende verhindern
  // den Eindruck einer geraden Allee.
  const int treeX[] = {18, 71, 139, 207, 284, 353, 424, 470};
  const int treeH[] = {118, 92, 132, 105, 125, 96, 138, 108};
  for (int i = 0; i < 8; ++i) {
    int x = treeX[i];
    int base = groundYAt((int16_t)x);
    int lean = (x - CX) / 18;
    int tx = x + lean;
    int top = base - treeH[i];

    gfx->fillRoundRect(x - 7, base - 4, 14, treeH[i] + 4, 5, trunk);
    gfx->fillCircle(tx, top + 18, 30, midTree);
    gfx->fillCircle(tx - 23, top + 38, 23, midTree);
    gfx->fillCircle(tx + 24, top + 39, 25, midTree);
    gfx->fillCircle(tx - 8, top - 1, 17, tree);
    gfx->fillCircle(tx + 15, top + 8, 20, tree);
  }

  // Dunkle Bodenvegetation am Rand des Pfades: unregelmaessig und bewusst
  // niedriger als der Spielbereich, damit Hindernisse klar lesbar bleiben.
  for (int x = 0; x < 466; x += 31) {
    int base = groundYAt((int16_t)x);
    int h = 9 + ((x * 5) % 15);
    gfx->fillCircle(x, base - h, 10, undergrowth);
    gfx->fillCircle(x + 9, base - h + 4, 8, undergrowth);
  }

  // Kleine Zweige/Laubspitzen am oberen Rand geben dem Wald mehr Tiefe.
  for (int x = 34; x < 450; x += 83) {
    int y = 108 + ((x * 5) % 42);
    gfx->drawLine(x, y + 18, x + 24, y, trunk);
    gfx->fillCircle(x + 27, y - 2, 10, tree);
    gfx->fillCircle(x + 36, y + 4, 8, tree);
  }

  // Kleine bewegte Wolken nur dezent im offenen Himmel.
  int cloud = (int)((millis() / 45) % 560) - 60;
  gfx->fillCircle(cloud, 82, 11, 0xFFFF);
  gfx->fillCircle(cloud + 16, 85, 9, 0xFFFF);
  gfx->fillCircle(cloud - 13, 87, 8, 0xFFFF);
}

void drawScore() {
  uint16_t ink = rgb565(24, 28, 38);
  uint16_t panel = rgb565(248, 248, 238);

  char score[20];
  snprintf(score, sizeof(score), "SCORE %04u", gScore);

  // Nur der aktuelle Score waehrend des Laufs; der Bestwert bleibt im Game Over.
  gfx->fillRoundRect(170, 50, 126, 42, 12, panel);
  gfx->drawRoundRect(170, 50, 126, 42, 12, ink);

  gfx->setTextColor(ink);
  gfx->setTextSize(2);
  gfx->setCursor(183, 64);
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
