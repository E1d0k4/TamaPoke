// TamaDash.cpp - "Tama Dash": Fuchs-Endless-Runner auf einem kleinen Planeten
//
// Grafische Grundlage: gekrümmte Planetenoberflaeche + lokales Koordinatensystem,
// damit Wald, Fuchs und Hindernisse der Weltkrümmung folgen.

#include "TamaDash.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>
#include <cstring>
#include "Arduino_GFX_Library.h"
#include "audio.h"

extern Arduino_Canvas *gfx;

namespace {

enum State : uint8_t { ST_RUNNING, ST_JUMPING, ST_GAMEOVER, ST_EXIT };
enum ObType : uint8_t { OB_NONE, OB_BUSH, OB_BRANCH };

struct Obstacle {
  ObType type;
  float x;
};

static constexpr int MAXOB = 4;
static constexpr float GRAV = 2700.0f;
static constexpr float JUMP_V = 820.0f;
static constexpr float FASTFALL = 1500.0f;

static Arduino_Canvas *g = nullptr;
static Preferences prefs;

static bool active = false;
static State st = ST_RUNNING;

static float W = 466.0f;
static float H = 466.0f;
static float sc = 1.0f;
static float cx = 233.0f;
static float R = 700.0f;
static float G = 300.0f;
static float foxX = 140.0f;

static float dist = 0.0f;
static float speed = 0.0f;
static float runTime = 0.0f;
static float jumpH = 0.0f;
static float jumpV = 0.0f;
static float nextGap = 0.0f;

static bool fastFell = false;
static bool newBest = false;

static uint32_t lastMs = 0;
static uint32_t overMs = 0;
static uint32_t lastTapMs = 0;
static uint32_t lastFrameMs = 0;
static uint32_t score = 0;
static uint32_t best = 0;
static uint32_t rngS = 2463534242u;

static Obstacle obs[MAXOB];

static uint8_t sunTaps = 0;
static uint32_t lastSunTapMs = 0;

static constexpr uint8_t TD_SUN_TAPS = 5;
static constexpr uint32_t TD_SUN_MAX_GAP_MS = 1000;
static constexpr uint32_t TD_GAMEOVER_IDLE_MS = 6000;

static constexpr uint16_t TD_WHITE = 0xFFFF;

static inline uint16_t rgb(uint8_t r, uint8_t gg, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((gg & 0xFC) << 3) | (b >> 3));
}

static uint32_t rnd() {
  rngS ^= rngS << 13;
  rngS ^= rngS >> 17;
  rngS ^= rngS << 5;
  return rngS;
}

static float frand() {
  return (rnd() & 0xFFFF) / 65535.0f;
}

static uint32_t hash32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352d;
  x ^= x >> 15;
  x *= 0x846ca68b;
  x ^= x >> 16;
  return x;
}

static inline int ri(float v) {
  return (int)lroundf(v);
}

// Leichte Parabel als Kugel-Naeherung.
static float groundY(float x) {
  float d = x - cx;
  return G + d * d / (2.0f * R);
}

// Lokales Koordinatensystem auf der Planetenoberflaeche.
// lx = Tangente, ly = "hoch" entlang der Oberflaechennormalen.
struct Frame {
  float bx;
  float by;
  float c;
  float s;
  float k;
};

static Frame frameAt(float x, float yOff, float k) {
  float d = x - cx;
  float n = sqrtf(R * R + d * d);
  Frame f;
  f.bx = x;
  f.by = groundY(x) + yOff;
  f.c = R / n;
  f.s = d / n;
  f.k = k;
  return f;
}

static void P(const Frame &f, float lx, float ly, int &ox, int &oy) {
  ox = ri(f.bx + (lx * f.c + ly * f.s) * f.k);
  oy = ri(f.by + (lx * f.s - ly * f.c) * f.k);
}

static void tri(const Frame &f, float x0, float y0, float x1, float y1,
                float x2, float y2, uint16_t col) {
  int ax, ay, bx, by, cx2, cy;
  P(f, x0, y0, ax, ay);
  P(f, x1, y1, bx, by);
  P(f, x2, y2, cx2, cy);
  g->fillTriangle(ax, ay, bx, by, cx2, cy, col);
}

static void quad(const Frame &f, float x0, float y0, float x1, float y1,
                 float x2, float y2, float x3, float y3, uint16_t col) {
  tri(f, x0, y0, x1, y1, x2, y2, col);
  tri(f, x0, y0, x2, y2, x3, y3, col);
}

static void circ(const Frame &f, float lx, float ly, float r, uint16_t col) {
  int x, y;
  P(f, lx, ly, x, y);
  int rr = ri(r * f.k);
  if (rr < 1) rr = 1;
  g->fillCircle(x, y, rr, col);
}

static void limb(const Frame &f, float x0, float y0, float x1, float y1,
                 float w, uint16_t col) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 0.01f) return;
  float nx = -dy / len * w * 0.5f;
  float ny = dx / len * w * 0.5f;
  quad(f, x0 + nx, y0 + ny, x0 - nx, y0 - ny,
       x1 - nx, y1 - ny, x1 + nx, y1 + ny, col);
}

static void loadBest() {
  prefs.begin("tamadash", true);
  best = prefs.getUInt("best", 0);
  prefs.end();
}

static void saveBest() {
  prefs.begin("tamadash", false);
  prefs.putUInt("best", best);
  prefs.end();
}

static void resetRun() {
  for (int i = 0; i < MAXOB; ++i) obs[i].type = OB_NONE;

  dist = 0;
  runTime = 0;
  jumpH = 0;
  jumpV = 0;
  fastFell = false;
  newBest = false;
  score = 0;

  speed = 240.0f * sc;
  nextGap = 320.0f * sc;
  st = ST_RUNNING;
  overMs = 0;
}

static void jump() {
  if (st == ST_GAMEOVER) {
    if (millis() - overMs > 700) resetRun();
    return;
  }

  if (st == ST_RUNNING) {
    jumpV = JUMP_V * sc;
    st = ST_JUMPING;
    fastFell = false;
    // Do not trigger a blocking/visually disruptive audio path from the
    // touch-to-jump event. The runner must keep the display frame sequence
    // uninterrupted while the jump starts.
    return;
  }

  if (st == ST_JUMPING && !fastFell) {
    jumpV = -FASTFALL * sc;
    fastFell = true;
  }
}

static void spawn() {
  int idx = -1;
  for (int i = 0; i < MAXOB; ++i) {
    if (obs[i].type == OB_NONE) {
      idx = i;
      break;
    }
  }
  if (idx < 0) return;

  // Erst nach kurzer Eingewoehnung kommen die niedrigen Aeste.
  ObType type = (score >= 15 && frand() < 0.40f) ? OB_BRANCH : OB_BUSH;
  obs[idx].type = type;
  obs[idx].x = W + (type == OB_BRANCH ? 140.0f : 40.0f) * sc;
  nextGap = speed * (0.95f + frand() * 0.80f)
          + (type == OB_BRANCH ? 120.0f : 50.0f) * sc;
}

static bool collides() {
  float left = foxX - 12.0f * sc;
  float right = foxX + 24.0f * sc;
  float bottom = jumpH;
  float top = jumpH + 44.0f * sc;

  for (int i = 0; i < MAXOB; ++i) {
    if (obs[i].type == OB_BUSH) {
      if (right > obs[i].x - 18.0f * sc &&
          left < obs[i].x + 18.0f * sc &&
          bottom < 24.0f * sc) {
        return true;
      }
    } else if (obs[i].type == OB_BRANCH) {
      // Ast + Blaetter blockieren den Sprungbereich.
      // Am Boden ist der Fuchs niedrig genug, um darunter hindurchzulaufen.
      if (right > obs[i].x - 90.0f * sc &&
          left < obs[i].x + 8.0f * sc &&
          top > 58.0f * sc) {
        return true;
      }
    }
  }
  return false;
}

static void gameOver() {
  st = ST_GAMEOVER;
  overMs = millis();

  if (score > best) {
    best = score;
    newBest = true;
    saveBest();
    sfxPlay(SFX_MEDAL);
  } else {
    sfxPlay(SFX_LEVEL);
  }
}

static void update(float dt) {
  if (st == ST_GAMEOVER || st == ST_EXIT) return;

  runTime += dt;
  speed = fminf(240.0f + 5.0f * runTime, 480.0f) * sc;
  dist += speed * dt;
  score = (uint32_t)(dist / (10.0f * sc));

  if (st == ST_JUMPING) {
    jumpV -= GRAV * sc * dt;
    jumpH += jumpV * dt;

    if (jumpH <= 0) {
      jumpH = 0;
      jumpV = 0;
      st = ST_RUNNING;
      fastFell = false;
    }
  }

  for (int i = 0; i < MAXOB; ++i) {
    if (obs[i].type == OB_NONE) continue;
    obs[i].x -= speed * dt;
    if (obs[i].x < -160.0f * sc) obs[i].type = OB_NONE;
  }

  nextGap -= speed * dt;
  if (nextGap <= 0) spawn();

  if (collides()) gameOver();
}

static void drawSky() {
  const uint8_t t0[3] = {110, 185, 235};
  const uint8_t t1[3] = {210, 238, 225};
  const int bands = 10;
  int hh = ri(G + 40.0f * sc);

  for (int i = 0; i < bands; ++i) {
    float u = (float)i / (bands - 1);
    uint16_t col = rgb(
      (uint8_t)(t0[0] + (t1[0] - t0[0]) * u),
      (uint8_t)(t0[1] + (t1[1] - t0[1]) * u),
      (uint8_t)(t0[2] + (t1[2] - t0[2]) * u)
    );
    int y0 = hh * i / bands;
    int y1 = hh * (i + 1) / bands;
    g->fillRect(0, y0, W, y1 - y0 + 1, col);
  }
}

static void drawBand(float up, uint16_t col, uint16_t grass, uint16_t soil) {
  int gh = ri(5.0f * sc);
  if (gh < 3) gh = 3;

  for (int x = 0; x < (int)W; x += 4) {
    int y = ri(groundY(x + 2.0f) - up);
    if (y >= (int)H) continue;
    if (y < 0) y = 0;

    g->fillRect(x, y, 4, (int)H - y, col);
    g->fillRect(x, y, 4, gh, grass);

    if (soil) {
      int sy = y + ri(26.0f * sc);
      if (sy < (int)H)
        g->fillRect(x, sy, 4, (int)H - sy, soil);
    }
  }
}

static void drawTree(float x, float baseUp, float t, uint8_t kind,
                     uint16_t trunkC, uint16_t c1, uint16_t c2) {
  Frame f = frameAt(x, -baseUp, sc * t);

  if (kind == 0) {
    float th = 60.0f;
    float w = 13.0f;
    float r = 34.0f;

    // Stamm und Krone teilen dasselbe gekruemmte Koordinatensystem.
    quad(f, -w / 2, -6, w / 2, -6, w * 0.35f, th,
         -w * 0.35f, th, trunkC);

    circ(f, -r * 0.8f, th + r * 0.1f, r * 0.75f, c1);
    circ(f,  r * 0.8f, th + r * 0.1f, r * 0.75f, c1);
    circ(f, 0, th + r * 0.6f, r, c1);
    circ(f, -r * 0.25f, th + r * 0.95f, r * 0.55f, c2);
  } else {
    float w = 10.0f;
    quad(f, -w / 2, -6, w / 2, -6, w * 0.4f, 45,
         -w * 0.4f, 45, trunkC);

    tri(f, -36, 28, 36, 28, 0, 82, c1);
    tri(f, -29, 58, 29, 58, 0, 112, c1);
    tri(f, -21, 88, 21, 88, 0, 138, c2);
  }
}

static void drawLayer(float factor, float cell, float baseUp,
                      float tmin, float tmax, int skipPct,
                      uint16_t trunkC, uint16_t c1, uint16_t c2,
                      uint32_t seed) {
  float off = dist * factor;
  float margin = 120.0f * sc;

  int i0 = (int)floorf((off - margin) / cell);
  int i1 = (int)floorf((off + W + margin) / cell);

  for (int i = i0; i <= i1; ++i) {
    uint32_t h = hash32((uint32_t)i * 0x9E3779B1u + seed);
    if ((int)(h % 100) < skipPct) continue;

    float wx = i * cell +
               ((h >> 8) & 0xFF) / 255.0f * cell * 0.9f;
    float t = tmin +
              ((h >> 16) & 0xFF) / 255.0f * (tmax - tmin);
    uint8_t kind = ((h >> 24) & 3) == 0 ? 1 : 0;

    drawTree(wx - off, baseUp, t, kind, trunkC, c1, c2);
  }
}

static void drawBush(float x) {
  Frame f = frameAt(x, 2.0f * sc, sc);
  uint16_t dk = rgb(35, 105, 50);
  uint16_t md = rgb(55, 145, 65);
  uint16_t bl = rgb(225, 40, 65);
  uint16_t br = rgb(105, 66, 38);

  circ(f, -14, 10, 13, dk);
  circ(f,  14, 10, 13, dk);
  circ(f, 0, 15, 15, md);
  circ(f, -8, 20, 9, md);
  circ(f, 9, 20, 9, md);

  quad(f, -18, 4, 18, 4, 17, -4, -17, -4, br);

  circ(f, -10, 14, 3, bl);
  circ(f, 3, 23, 3, bl);
  circ(f, 14, 13, 3, bl);
  circ(f, -1, 9, 3, bl);
  circ(f, 9, 20, 2.5f, bl);
}

static void drawBranchTree(float tx) {
  Frame f = frameAt(tx, 4.0f * sc, sc);
  uint16_t tr = rgb(105, 66, 38);
  uint16_t c1 = rgb(40, 120, 55);
  uint16_t c2 = rgb(75, 165, 75);

  // Stamm kommt aus dem Boden und bleibt in derselben Oberflaechenrichtung.
  quad(f, -9, -6, 9, -6, 7, 150, -7, 150, tr);

  // Der Ast haengt tief genug in den Laufweg.
  quad(f, -4, 78, -4, 62, -92, 66, -92, 74, tr);

  circ(f, 0, 160, 38, c1);
  circ(f, -26, 148, 28, c1);
  circ(f, 26, 148, 28, c1);

  circ(f, -86, 70, 12, c1);
  circ(f, -80, 80, 18, c1);
  circ(f, -55, 92, 30, c1);
  circ(f, -25, 100, 34, c1);
  circ(f, -40, 108, 20, c2);
}

static void drawFox() {
  Frame f = frameAt(foxX, -jumpH, sc * 0.8f);
  uint16_t org = rgb(242, 128, 42);
  uint16_t dko = rgb(200, 92, 25);
  uint16_t cre = rgb(255, 242, 218);
  uint16_t brn = rgb(62, 36, 26);

  float ph = runTime * 14.0f;
  float wag = sinf(runTime * 9.0f) * 4.0f;
  bool air = jumpH > 0.5f;

  auto leg = [&](float hx, float phase, uint16_t col) {
    float fxo;
    float fyo;

    if (air) {
      fxo = hx + (hx > 0 ? 11 : -11);
      fyo = 5;
    } else {
      // Der Koerper bleibt unveraendert; nur die Beinbewegung wird umgekehrt.
      fxo = hx - sinf(ph + phase) * 9;
      fyo = fmaxf(0.0f, -cosf(ph + phase)) * 6;
    }

    limb(f, hx, 17, fxo, fyo + 3, 5.5f, col);
    circ(f, fxo, fyo + 3, 3.3f, brn);
  };

  // Langer, klarer buschiger Schwanz nach hinten.
  tri(f, -10, 31, -10, 14, -52, 36 + wag, org);
  circ(f, -28, 26, 10, org);
  circ(f, -40, 31 + wag * 0.5f, 10.5f, org);
  circ(f, -52, 36 + wag, 8.5f, cre);

  leg(-9, 3.14159f, dko);
  leg(13, 0, dko);

  circ(f, -10, 24, 10.5f, org);
  circ(f, 10, 24, 10.5f, org);
  quad(f, -10, 13.5f, 10, 13.5f, 10, 34.5f, -10, 34.5f, org);
  circ(f, 13, 25, 6, cre);

  circ(f, 22, 38, 12, org);
  tri(f, 28, 45, 30, 30, 43, 36, org);
  tri(f, 23, 30, 32, 27, 41, 34, cre);
  circ(f, 43, 36, 2.8f, brn);

  tri(f, 11, 46, 13, 67, 24, 50, org);
  tri(f, 19, 49, 29, 68, 32, 46, org);
  tri(f, 14, 49, 15, 60, 21, 51, brn);
  tri(f, 23, 51, 28, 61, 29, 48, brn);

  circ(f, 26, 41, 3.4f, brn);
  circ(f, 27, 42.3f, 1.2f, cre);

  leg(-13, 0, brn);
  leg(10, 3.14159f, brn);
}

static void textCentered(const char *s, float cxx, float y,
                         uint8_t size, uint16_t col) {
  int w = (int)strlen(s) * 6 * size;
  g->setTextSize(size);
  g->setTextColor(col);
  g->setCursor(ri(cxx - w / 2.0f), ri(y));
  g->print(s);
}

static void drawBackArrow() {
  int x = ri(cx);
  int y = ri(30.0f * sc);
  int r = ri(17.0f * sc);
  uint16_t bg = rgb(25, 45, 35);

  g->fillCircle(x, y, r, bg);
  g->fillRect(x - ri(8 * sc), y - ri(1.5f * sc),
              ri(18 * sc), ri(3 * sc) + 1, TD_WHITE);
  g->fillTriangle(x - ri(10 * sc), y,
                  x - ri(2 * sc), y - ri(8 * sc),
                  x - ri(2 * sc), y + ri(8 * sc), TD_WHITE);
}

static void drawHud() {
  drawBackArrow();

  uint8_t ts = (W >= 400) ? 3 : 2;
  char buf[24];

  if (st != ST_GAMEOVER) {
    snprintf(buf, sizeof(buf), "SCORE %04lu", (unsigned long)score);
    int tw = (int)strlen(buf) * 6 * ts;
    int th = 8 * ts;
    int px = ri(cx) - tw / 2 - 12;
    int py = ri(60 * sc);

    g->fillRoundRect(px, py, tw + 24, th + 12, 10,
                     rgb(25, 45, 35));
    g->setTextSize(ts);
    g->setTextColor(TD_WHITE);
    g->setCursor(px + 12, py + 6);
    g->print(buf);
    return;
  }

  int pw = ri(W * 0.66f);
  int ph = ri(H * 0.46f);
  int px = ri(cx) - pw / 2;
  int py = ri(H * 0.20f);

  g->fillRoundRect(px, py, pw, ph, 16, rgb(25, 45, 35));
  g->drawRoundRect(px, py, pw, ph, 16, rgb(255, 190, 90));

  float y = py + 16;
  textCentered("GAME OVER", cx, y, ts + 1, rgb(255, 150, 60));
  y += 8 * (ts + 1) + 14;

  snprintf(buf, sizeof(buf), "SCORE: %04lu", (unsigned long)score);
  textCentered(buf, cx, y, ts, TD_WHITE);
  y += 8 * ts + 8;

  snprintf(buf, sizeof(buf), "BEST: %04lu", (unsigned long)best);
  textCentered(buf, cx, y, ts, rgb(255, 225, 120));
  y += 8 * ts + 8;

  if (newBest) {
    textCentered("NEW BEST!", cx, y, ts - 1, rgb(120, 230, 120));
  }

  y = py + ph - 8 * (ts - 1) - 14;
  textCentered("TAP TO RETRY", cx, y, ts - 1, rgb(200, 220, 210));
}

static void draw() {
  drawSky();

  // Mehrere Tiefenebenen erzeugen einen Wald statt einer regelmaessigen Allee.
  drawBand(26 * sc, rgb(78, 138, 112), rgb(95, 155, 128), 0);
  drawLayer(0.35f, 46 * sc, 26 * sc, 0.45f, 0.70f, 20,
            rgb(80, 100, 95), rgb(70, 130, 110), rgb(95, 158, 132),
            0x1111u);

  drawBand(13 * sc, rgb(58, 122, 82), rgb(78, 145, 95), 0);
  drawLayer(0.65f, 62 * sc, 13 * sc, 0.65f, 0.95f, 22,
            rgb(92, 64, 44), rgb(40, 108, 68), rgb(62, 138, 88),
            0x2222u);

  drawBand(0, rgb(70, 150, 60), rgb(105, 190, 72), rgb(112, 82, 52));
  drawLayer(1.00f, 120 * sc, 0, 0.95f, 1.30f, 40,
            rgb(108, 68, 40), rgb(40, 125, 55), rgb(72, 162, 72),
            0x3333u);

  // Kleine Grasdetails entlang des Weges.
  uint16_t grass = rgb(70, 150, 70);
  for (int x = 5; x < (int)W; x += 38) {
    int base = ri(groundY(x));
    g->drawLine(x, base, x - 4, base - 10, grass);
    g->drawLine(x + 4, base, x + 7, base - 13, grass);
    g->drawLine(x + 8, base, x + 12, base - 8, grass);
  }

  // Dezente Wolken.
  int cloud = (int)((millis() / 45) % 560) - 60;
  uint16_t cloudC = rgb(250, 252, 246);
  g->fillCircle(cloud, 82, 11, cloudC);
  g->fillCircle(cloud + 16, 85, 9, cloudC);
  g->fillCircle(cloud - 13, 87, 8, cloudC);

  for (int i = 0; i < MAXOB; ++i) {
    if (obs[i].type == OB_BUSH) drawBush(obs[i].x);
    else if (obs[i].type == OB_BRANCH) drawBranchTree(obs[i].x);
  }

  drawFox();
  drawHud();
}

} // namespace

void tamaDashOpen() {
  if (active) return;

  g = gfx;
  W = (float)gfx->width();
  H = (float)gfx->height();
  sc = W / 466.0f;
  cx = W / 2.0f;
  R = 1.5f * W;
  G = H * 0.64f;
  foxX = W * 0.30f;

  loadBest();
  rngS ^= micros();
  resetRun();
  active = true;
  lastMs = millis();
  lastFrameMs = lastMs;
  lastTapMs = 0;
}

void tamaDashResetEasterEgg() {
  sunTaps = 0;
  lastSunTapMs = 0;
}

bool tamaDashHandleSunTap(int16_t x, int16_t y) {
  // Bestehendes Sonnen-Symbol auf der Helligkeitsseite.
  const int dx = x - 233;
  const int dy = y - 140;
  if (dx * dx + dy * dy > 34 * 34) {
    sunTaps = 0;
    lastSunTapMs = 0;
    return false;
  }

  uint32_t now = millis();
  if (sunTaps > 0 && now - lastSunTapMs > TD_SUN_MAX_GAP_MS) {
    sunTaps = 0;
  }

  lastSunTapMs = now;

  ++sunTaps;
  if (sunTaps < TD_SUN_TAPS) {
    return false;
  }

  sunTaps = 0;
  lastSunTapMs = 0;
  return true;
}

void tamaDashTap(int16_t x, int16_t y) {
  if (!active) return;

  // Groessere unsichtbare Touchflaeche fuer den kleinen Pfeil.
  if (y < 66 * sc && fabsf(x - cx) < 55 * sc) {
    st = ST_EXIT;
    return;
  }

  uint32_t now = millis();
  if (now - lastTapMs < 50) return;
  lastTapMs = now;

  jump();
}

bool tamaDashRender() {
  if (!active) return false;

  if (st == ST_EXIT) {
    active = false;
    st = ST_RUNNING;
    tamaDashResetEasterEgg();
    g = nullptr;
    return false;
  }

  uint32_t now = millis();

  // Nach einem Game Over nicht minutenlang auf dem Bildschirm stehen bleiben.
  // Ein Touch kann weiterhin direkt einen neuen Lauf starten; ohne Eingabe
  // kehren wir nach kurzer Pause automatisch zum Hauptbildschirm zurueck.
  if (st == ST_GAMEOVER && now - overMs >= TD_GAMEOVER_IDLE_MS) {
    active = false;
    st = ST_RUNNING;
    tamaDashResetEasterEgg();
    g = nullptr;
    return false;
  }

  // The main TamaPoke loop deliberately limits full-screen flushes to avoid
  // overlapping QSPI/DMA transfers. Keep one authoritative frame timestamp
  // here as well, so Tama Dash never renders twice for the same scheduler tick.
  if (now == lastFrameMs) return true;
  lastFrameMs = now;

  float dt = (now - lastMs) / 1000.0f;
  if (dt > 0.05f) dt = 0.05f;
  lastMs = now;

  update(dt);
  draw();
  g->flush();
  return true;
}


bool tamaDashActive() {
  return active;
}

uint32_t tamaDashBestScore() {
  loadBest();
  return best;
}

void tamaDashSetBestScore(uint32_t value) {
  best = value;
  saveBest();
}
