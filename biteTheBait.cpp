// ============================================================
// Bite the Bait  –  SFML Version  (moving entities + direction)
// Compile: g++ main.cpp -o game -lsfml-graphics -lsfml-window -lsfml-system -std=c++17
// ============================================================

#include <SFML/Graphics.hpp>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>

using namespace std;

// ── Constants ────────────────────────────────────────────────
const int   GRID_SIZE = 10;
const int   CELL_SIZE = 56;
const int   PANEL_WIDTH = 220;
const int   WINDOW_W = GRID_SIZE * CELL_SIZE + PANEL_WIDTH;
const int   WINDOW_H = GRID_SIZE * CELL_SIZE;
const float WORLD_W = (float)(GRID_SIZE * CELL_SIZE);
const float WORLD_H = (float)(GRID_SIZE * CELL_SIZE);
const int   MAX_LIVES = 3;
const int   MAX_SCORES = 5;

// max counts of each entity alive at once
const int MAX_FOOD = 5;
const int MAX_PRED = 3;
const int MAX_POWER = 1;

// pixel speeds (pixels per second)
const float FOOD_SPEED = 28.f;
const float PRED_SPEED = 40.f;
const float POWER_SPEED = 20.f;

// Player facing directions
const int DIR_RIGHT = 0;
const int DIR_LEFT = 1;
const int DIR_UP = 2;
const int DIR_DOWN = 3;

// Palette
const sf::Color COL_WATER(10, 90, 140);
const sf::Color COL_GRID(15, 105, 160, 55);
const sf::Color COL_PANEL(8, 30, 55);
const sf::Color COL_TEXT(220, 240, 255);
const sf::Color COL_ACCENT(80, 200, 255);
const sf::Color COL_FOOD(80, 220, 80);
const sf::Color COL_PRED(220, 50, 50);
const sf::Color COL_POWER(255, 200, 40);

// ── Entity ────────────────────────────────────────────────────
struct Entity {
    float x = 0, y = 0;
    float vx = 0, vy = 0;
    char  type = 'F';
    bool  alive = false;
    float wiggle = 0.f;   // per-entity tail phase
};

// ── Player ───────────────────────────────────────────────────
struct Player {
    float x = 0, y = 0;
    int   gridX = 0, gridY = 0;
    int   size = 1;
    int   score = 0;
    int   lives = MAX_LIVES;
    int   facing = DIR_RIGHT;
    float animAngle = 0.f;
    float targetX = 0, targetY = 0;
    bool  moving = false;
    float hitTimer = 0.f;
};

// ── Decoration ───────────────────────────────────────────────
struct Decor {
    float x, y, scale, phase;
    int   type; // 0=rock  1=seagrass
};

// ── Scoreboard ───────────────────────────────────────────────
struct ScoreEntry { int score = 0; };
ScoreEntry scoreboard[MAX_SCORES];
int scoreboardCount = 0;

void loadScoreboard() {
    ifstream f("scoreboard.txt");
    scoreboardCount = 0;
    if (f.is_open())
        while (f >> scoreboard[scoreboardCount].score && scoreboardCount < MAX_SCORES)
            scoreboardCount++;
}
void saveScoreboard() {
    ofstream f("scoreboard.txt");
    if (f.is_open())
        for (int i = 0; i < scoreboardCount; i++)
            f << scoreboard[i].score << "\n";
}
void addScore(int s) {
    if (scoreboardCount < MAX_SCORES) {
        scoreboard[scoreboardCount++].score = s;
    }
    else {
        int mi = 0;
        for (int i = 1; i < MAX_SCORES; i++)
            if (scoreboard[i].score < scoreboard[mi].score) mi = i;
        if (s > scoreboard[mi].score) scoreboard[mi].score = s;
    }
    for (int i = 0; i < scoreboardCount - 1; i++)
        for (int j = i + 1; j < scoreboardCount; j++)
            if (scoreboard[j].score > scoreboard[i].score)
                swap(scoreboard[i], scoreboard[j]);
}

// ── Helpers ──────────────────────────────────────────────────
float randF(float lo, float hi) {
    return lo + (hi - lo) * ((float)rand() / RAND_MAX);
}

sf::RectangleShape makeRect(float x, float y, float w, float h,
    sf::Color fill, float ot = 0.f,
    sf::Color oc = sf::Color::Transparent) {
    sf::RectangleShape r({ w,h });
    r.setPosition(x, y); r.setFillColor(fill);
    r.setOutlineThickness(ot); r.setOutlineColor(oc);
    return r;
}

// ── Spawn entity from a random edge ──────────────────────────
void spawnEntity(Entity& e, char type) {
    e.type = type;
    e.alive = true;
    e.wiggle = randF(0.f, 6.28f);

    float speed = (type == 'X') ? PRED_SPEED
        : (type == '!') ? POWER_SPEED
        : FOOD_SPEED;

    float margin = 20.f;
    // Only spawn from left or right edge — pure horizontal movement
    if (rand() % 2 == 0) {
        // From LEFT → moves RIGHT
        e.x = -margin;
        e.y = randF(margin, WORLD_H - margin);
        e.vx = speed;
        e.vy = 0.f;          // ← no vertical drift
    }
    else {
        // From RIGHT → moves LEFT
        e.x = WORLD_W + margin;
        e.y = randF(margin, WORLD_H - margin);
        e.vx = -speed;
        e.vy = 0.f;          // ← no vertical drift
    }
}
// ── Game state ───────────────────────────────────────────────
struct GameState {
    Player         player;
    vector<Entity> entities;
    vector<Decor>  decors;
    float          spawnTimer = 0.f;

    void init() {
        player = Player();
        player.gridX = GRID_SIZE / 2;
        player.gridY = GRID_SIZE / 2;
        player.x = player.gridY * CELL_SIZE + CELL_SIZE * 0.5f;
        player.y = player.gridX * CELL_SIZE + CELL_SIZE * 0.5f;
        player.targetX = player.x;
        player.targetY = player.y;
        player.size = 1;
        player.score = 0;
        player.lives = MAX_LIVES;
        player.facing = DIR_RIGHT;
        player.moving = false;
        entities.clear();
        spawnTimer = 0.f;

        // --- Decorations ---
        decors.clear();
        // Rocks on the bottom strip
        for (int i = 0; i < 7; i++) {
            Decor d;
            d.type = 0;
            d.scale = randF(0.45f, 1.05f);
            d.phase = randF(0.f, 6.28f);
            d.x = randF(14.f, WORLD_W - 14.f);
            d.y = randF(WORLD_H * 0.80f, WORLD_H - 6.f);
            decors.push_back(d);
        }
        // Seagrass along the bottom
        for (int i = 0; i < 11; i++) {
            Decor d;
            d.type = 1;
            d.scale = randF(0.6f, 1.4f);
            d.phase = randF(0.f, 6.28f);
            d.x = 10.f + (float)i * (WORLD_W / 11.f) + randF(-6.f, 6.f);
            d.y = WORLD_H - 4.f;
            decors.push_back(d);
        }
    }
};

// ── Draw seagrass ─────────────────────────────────────────────
void drawSeagrass(sf::RenderWindow& win, float x, float y, float scale, float sway) {
    float h = 24.f * scale;
    float sw = sway * 6.f * scale;
    for (int b = -1; b <= 1; b++) {
        float bx = x + (float)b * 7.f * scale;
        sf::ConvexShape blade;
        blade.setPointCount(4);
        blade.setPoint(0, { -2.2f * scale, 0 });
        blade.setPoint(1, { 2.2f * scale, 0 });
        blade.setPoint(2, { 2.2f * scale + sw, -h });
        blade.setPoint(3, { -2.2f * scale + sw, -h });
        blade.setFillColor(sf::Color(20 + b * 15, 155 + b * 10, 50, 210));
        blade.setPosition(bx, y);
        win.draw(blade);
    }
}

// ── Draw rock ────────────────────────────────────────────────
void drawRock(sf::RenderWindow& win, float x, float y, float scale) {
    sf::CircleShape r(15.f * scale, 8);
    r.setFillColor(sf::Color(72, 72, 84, 230));
    r.setOutlineColor(sf::Color(105, 105, 118, 200));
    r.setOutlineThickness(1.5f);
    r.setOrigin(15.f * scale, 15.f * scale);
    r.setScale(1.f, 0.58f);
    r.setPosition(x, y);
    win.draw(r);
    sf::CircleShape hi(4.5f * scale, 6);
    hi.setFillColor(sf::Color(135, 135, 150, 110));
    hi.setOrigin(4.5f * scale, 4.5f * scale);
    hi.setScale(1.f, 0.58f);
    hi.setPosition(x - 5.f * scale, y - 4.f * scale);
    win.draw(hi);
}

// ── Core fish drawing ─────────────────────────────────────────
// All fish drawn pointing RIGHT in local space, then rotated.
// cx,cy = centre pixel; rad = body radius;
// facing: DIR_RIGHT/LEFT/UP/DOWN; waggle = tail wiggle pixels
void drawFishAt(sf::RenderWindow& win,
    float cx, float cy, float rad,
    int facing, float waggle,
    sf::Color bodyCol, sf::Color tailCol, sf::Color eyeCol,
    bool spines = false, bool glow = false,
    float outlineW = 0.f, sf::Color outlineCol = sf::Color::Transparent)
{
    float angle = 180.f;                      // default = facing LEFT (comes from right)
    if (facing == DIR_LEFT)  angle = 0.f;     // comes from left → faces right
    if (facing == DIR_UP)    angle = 270.f;
    if (facing == DIR_DOWN)  angle = 90.f;

    sf::Transform tf;
    tf.translate(cx, cy).rotate(angle);

    // Glow halo
    if (glow) {
        sf::CircleShape halo(rad + 8.f, 32);
        halo.setFillColor(sf::Color(bodyCol.r, bodyCol.g, bodyCol.b, 30));
        halo.setOrigin(rad + 8.f, rad + 8.f);
        win.draw(halo, tf);
    }

    // Tail
    sf::ConvexShape tail;
    tail.setPointCount(3);
    tail.setPoint(0, { 0.f, 0.f });
    tail.setPoint(1, { rad * 0.95f,  rad * 0.58f + waggle });
    tail.setPoint(2, { rad * 0.95f, -rad * 0.58f + waggle });
    tail.setFillColor(tailCol);
    tail.setPosition(rad * 0.82f, 0.f);
    win.draw(tail, tf);

    // Body
    sf::CircleShape body(rad, 32);
    body.setFillColor(bodyCol);
    if (outlineW > 0.f) {
        body.setOutlineThickness(outlineW);
        body.setOutlineColor(outlineCol);
    }
    body.setOrigin(rad, rad);
    win.draw(body, tf);

    // Dorsal fin
    sf::ConvexShape fin;
    fin.setPointCount(3);
    fin.setPoint(0, { -rad * 0.12f, -rad * 0.06f });
    fin.setPoint(1, { -rad * 0.22f, -rad * 0.78f });
    fin.setPoint(2, { rad * 0.28f, -rad * 0.06f });
    fin.setFillColor(tailCol);
    win.draw(fin, tf);

    // Spines for predator
    if (spines) {
        for (int s = 0; s < 3; s++) {
            sf::RectangleShape spine({ 1.8f, rad * 0.52f });
            spine.setFillColor(sf::Color(170, 15, 15));
            spine.setPosition(-rad * 0.18f + (float)s * rad * 0.22f, -rad * 1.15f);
            win.draw(spine, tf);
        }
    }

    // Eye white
    sf::CircleShape eye(rad * 0.21f);
    eye.setFillColor(sf::Color::White);
    eye.setOrigin(rad * 0.21f, rad * 0.21f);
    eye.setPosition(-rad * 0.36f, -rad * 0.26f);
    win.draw(eye, tf);

    // Pupil
    sf::CircleShape pupil(rad * 0.10f);
    pupil.setFillColor(eyeCol);
    pupil.setOrigin(rad * 0.10f, rad * 0.10f);
    pupil.setPosition(-rad * 0.40f, -rad * 0.26f);
    win.draw(pupil, tf);
}

// ── Power-up star ────────────────────────────────────────────
void drawPowerUp(sf::RenderWindow& win, float cx, float cy, float t) {
    float pulse = 1.f + 0.14f * sinf(t * 2.8f);
    float r = 14.f * pulse;
    sf::CircleShape star(r, 5);
    star.setFillColor(COL_POWER);
    star.setOutlineColor(sf::Color(190, 120, 0));
    star.setOutlineThickness(2.f);
    star.setOrigin(r, r);
    star.setPosition(cx, cy);
    star.setRotation(54.f + t * 55.f);
    win.draw(star);
    sf::CircleShape inner(r * 0.48f, 5);
    inner.setFillColor(sf::Color(255, 238, 130));
    inner.setOrigin(r * 0.48f, r * 0.48f);
    inner.setPosition(cx, cy);
    inner.setRotation(54.f + t * 55.f);
    win.draw(inner);
}

// ── Facing from velocity ──────────────────────────────────────
int facingFromVel(float vx, float vy) {
    return (vx >= 0.f) ? DIR_RIGHT : DIR_LEFT;
}

// ── Count alive entities of a type ───────────────────────────
int countAlive(const vector<Entity>& ents, char type) {
    int n = 0;
    for (auto& e : ents) if (e.alive && e.type == type) n++;
    return n;
}

// ── Draw Background (water + grid + decorations) ──────────────
void drawBackground(sf::RenderWindow& win, const GameState& gs, float t) {
    win.draw(makeRect(0, 0, WORLD_W, WORLD_H, COL_WATER));

    for (int i = 0; i <= GRID_SIZE; i++) {
        sf::RectangleShape hl({ WORLD_W,1.f });
        hl.setPosition(0, (float)i * CELL_SIZE);
        hl.setFillColor(COL_GRID); win.draw(hl);
        sf::RectangleShape vl({ 1.f,WORLD_H });
        vl.setPosition((float)i * CELL_SIZE, 0);
        vl.setFillColor(COL_GRID); win.draw(vl);
    }

    // Seagrass (draw before rocks so rocks sit on top)
    for (auto& d : gs.decors)
        if (d.type == 1)
            drawSeagrass(win, d.x, d.y, d.scale, sinf(t * 1.3f + d.phase));

    for (auto& d : gs.decors)
        if (d.type == 0)
            drawRock(win, d.x, d.y, d.scale);
}

// ── Draw all entities ─────────────────────────────────────────

void drawEntities(sf::RenderWindow& win, const vector<Entity>& ents, float t, int playerSize) {
    for (auto& e : ents) {
        if (!e.alive) continue;
        float wag = sinf(t * 7.f + e.wiggle) * 4.f;
        int   face = facingFromVel(e.vx, e.vy);
        float playerRad = 14.f + (float)(playerSize - 1) * 2.5f;

        if (e.type == 'F') {
            // Food is always 60% of player size — shrinks as player grows
            float foodRad = playerRad * 0.60f;
            drawFishAt(win, e.x, e.y, foodRad, face, wag,
                sf::Color(60, 200, 60), sf::Color(30, 145, 30),
                sf::Color(5, 25, 5));
        }
        else if (e.type == 'X') {
            // Enemy is always 40% bigger than player — scales up with player
            float predRad = playerRad * 1.40f;
            drawFishAt(win, e.x, e.y, predRad, face, wag,
                COL_PRED, sf::Color(150, 15, 15),
                sf::Color(0, 0, 0), true);
        }


        else if (e.type == '!') {
            drawPowerUp(win, e.x, e.y, t);
        }
    }
}

// ── Draw player ──────────────────────────────────────────────
void drawPlayer(sf::RenderWindow& win, const Player& p) {
    float rad = 14.f + (float)(p.size - 1) * 2.5f;
    float wag = sinf(p.animAngle) * 5.5f;

    if (p.hitTimer > 0.f) {
        // Blink: skip drawing on alternating frames
        float blinkRate = (p.hitTimer > 1.0f) ? 8.f : 5.f; // fast then slow blink
        if (fmodf(p.hitTimer, 1.f / blinkRate) < (0.5f / blinkRate)) return; // invisible frame

        // Ghost / blur glow — draw faded copies offset slightly
        for (int g = 0; g < 3; g++) {
            float offset = (float)(g + 1) * 3.5f;
            sf::Uint8 alpha = (sf::Uint8)(60 - g * 18);
            drawFishAt(win, p.x + offset, p.y, rad, p.facing, wag,
                sf::Color(100, 200, 255, alpha),
                sf::Color(45, 145, 215, alpha),
                sf::Color(10, 20, 80, alpha),
                false, false, 0.f);
        }
    }

    // Draw main player (full opacity always when visible)
    drawFishAt(win, p.x, p.y, rad, p.facing, wag,
        sf::Color(100, 200, 255), sf::Color(45, 145, 215),
        sf::Color(10, 20, 80),
        false, true, 2.f, sf::Color(175, 225, 255));
}

// ── HUD ──────────────────────────────────────────────────────
void drawHUD(sf::RenderWindow& win, const Player& p, const sf::Font& font) {
    float hx = WORLD_W, pw = (float)PANEL_WIDTH, ph = (float)WINDOW_H;
    win.draw(makeRect(hx, 0, pw, ph, COL_PANEL));
    win.draw(makeRect(hx, 0, 3.f, ph, COL_ACCENT));

    auto lbl = [&](const string& s, float y, sf::Color col = COL_TEXT, unsigned sz = 16) {
        sf::Text t; t.setFont(font); t.setString(s);
        t.setCharacterSize(sz); t.setFillColor(col);
        t.setPosition(hx + 16.f, y); win.draw(t);
        };

    lbl("BITE THE BAIT", 18, COL_ACCENT, 18);
    win.draw(makeRect(hx + 10, 50, pw - 20, 2, sf::Color(40, 80, 120)));

    lbl("SCORE", 68, COL_ACCENT, 13);
    lbl(to_string(p.score), 86, COL_TEXT, 28);

    lbl("SIZE", 126, COL_ACCENT, 13);
    lbl(to_string(p.size), 144, COL_TEXT, 28);

    lbl("LIVES", 190, COL_ACCENT, 13);
    for (int i = 0; i < MAX_LIVES; i++) {
        sf::CircleShape h(9.f);
        h.setFillColor(i < p.lives ? sf::Color(220, 60, 80) : sf::Color(50, 50, 70));
        h.setPosition(hx + 16.f + (float)i * 26.f, 218.f);
        win.draw(h);
    }

    win.draw(makeRect(hx + 10, 256, pw - 20, 2, sf::Color(40, 80, 120)));
    lbl("LEGEND", 268, COL_ACCENT, 13);

    auto dot = [&](sf::Color c, float y) {
        sf::CircleShape ci(6.f); ci.setFillColor(c);
        ci.setPosition(hx + 16.f, y); win.draw(ci);
        };
    dot(COL_FOOD, 292); lbl("  Food  +10pts", 292, COL_TEXT, 13);
    dot(COL_PRED, 313); lbl("  Predator -1hp", 313, COL_TEXT, 13);
    dot(COL_POWER, 334); lbl("  Extra life", 334, COL_TEXT, 13);

    win.draw(makeRect(hx + 10, 360, pw - 20, 2, sf::Color(40, 80, 120)));
    lbl("CONTROLS", 373, COL_ACCENT, 13);
    lbl("W / Arrow Up", 393, COL_TEXT, 12);
    lbl("S / Arrow Down", 409, COL_TEXT, 12);
    lbl("A / Arrow Left", 425, COL_TEXT, 12);
    lbl("D / Arrow Right", 441, COL_TEXT, 12);
    lbl("Esc  - Menu", 459, COL_TEXT, 12);
}

// ── Screens ──────────────────────────────────────────────────
enum class Screen { MENU, GAME, INSTRUCTIONS, SCOREBOARD, GAMEOVER };

void drawMenu(sf::RenderWindow& win, const sf::Font& font, int sel) {
    win.draw(makeRect(0, 0, WINDOW_W, WINDOW_H, COL_PANEL));
    for (int i = 0; i < 8; i++) {
        sf::CircleShape c((float)(28 + i * 15), 32);
        c.setFillColor(sf::Color(10, 60, 110, (sf::Uint8)(22 + i * 5)));
        c.setPosition((float)i * 80 - 18.f, WINDOW_H - 115.f);
        win.draw(c);
    }
    {
        sf::Text t; t.setFont(font); t.setString("BITE THE BAIT");
        t.setCharacterSize(44); t.setFillColor(COL_ACCENT); t.setStyle(sf::Text::Bold);
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, 80); win.draw(t);
    }
    {
        sf::Text t; t.setFont(font); t.setString("~ A fish survival adventure ~");
        t.setCharacterSize(16); t.setFillColor(sf::Color(140, 180, 220));
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, 138); win.draw(t);
    }

    vector<string> opts = { "  Start Game","  Instructions","  Scoreboard","  Exit" };
    for (int i = 0; i < (int)opts.size(); i++) {
        float oy = 210.f + (float)i * 68.f; bool s = (i == sel);
        sf::RectangleShape btn({ 320.f,52.f }); btn.setOrigin(160.f, 0.f);
        btn.setPosition(WINDOW_W / 2.f, oy);
        btn.setFillColor(s ? sf::Color(20, 90, 160) : sf::Color(14, 50, 90));
        btn.setOutlineColor(s ? COL_ACCENT : sf::Color(30, 70, 110));
        btn.setOutlineThickness(s ? 2.f : 1.f); win.draw(btn);
        sf::Text opt; opt.setFont(font); opt.setString(opts[i]);
        opt.setCharacterSize(20); opt.setFillColor(s ? COL_ACCENT : COL_TEXT);
        auto ob = opt.getLocalBounds(); opt.setOrigin(ob.width / 2, ob.height / 2);
        opt.setPosition(WINDOW_W / 2.f, oy + 26.f); win.draw(opt);
    }
    {
        sf::Text t; t.setFont(font);
        t.setString("Use W/S or Arrow keys to navigate, Enter to select");
        t.setCharacterSize(13); t.setFillColor(sf::Color(80, 120, 170));
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, WINDOW_H - 36.f); win.draw(t);
    }
}

void drawInstructions(sf::RenderWindow& win, const sf::Font& font) {
    win.draw(makeRect(0, 0, WINDOW_W, WINDOW_H, COL_PANEL));
    auto clbl = [&](const string& s, float y, sf::Color col, unsigned sz) {
        sf::Text t; t.setFont(font); t.setString(s);
        t.setCharacterSize(sz); t.setFillColor(col);
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, y); win.draw(t);
        };
    clbl("INSTRUCTIONS", 60, COL_ACCENT, 34);
    vector<pair<string, string>> lines = {
        {"Move",        "W / A / S / D  or  Arrow Keys"},
        {"Food",        "Green fish — eat to grow  +10 pts"},
        {"Predator",    "Red fish — avoid! costs 1 life"},
        {"Power-Up",    "Gold star — grants an extra life"},
        {"Goal",        "Survive as long as possible!"},
    };
    float y = 140;
    for (int i = 0; i < (int)lines.size(); i++) {
        sf::Text k; k.setFont(font); k.setString(lines[i].first + ":");
        k.setCharacterSize(18); k.setFillColor(COL_ACCENT);
        k.setPosition(WINDOW_W * 0.18f, y); win.draw(k);
        sf::Text v; v.setFont(font); v.setString(lines[i].second);
        v.setCharacterSize(18); v.setFillColor(COL_TEXT);
        v.setPosition(WINDOW_W * 0.40f, y); win.draw(v);
        y += 52.f;
    }
    clbl("Press Esc to return", WINDOW_H - 50.f, sf::Color(80, 120, 170), 14);
}

void drawScoreboard(sf::RenderWindow& win, const sf::Font& font) {
    win.draw(makeRect(0, 0, WINDOW_W, WINDOW_H, COL_PANEL));
    {
        sf::Text t; t.setFont(font); t.setString("SCOREBOARD");
        t.setCharacterSize(34); t.setFillColor(COL_ACCENT);
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, 60); win.draw(t);
    }
    for (int i = 0; i < scoreboardCount; i++) {
        float y = 150.f + (float)i * 60.f;
        sf::RectangleShape row({ 400.f,44.f }); row.setOrigin(200.f, 0.f);
        row.setPosition(WINDOW_W / 2.f, y);
        row.setFillColor(sf::Color(10, 50, 90, 180));
        row.setOutlineColor(sf::Color(30, 80, 140)); row.setOutlineThickness(1.f);
        win.draw(row);
        sf::Text rank; rank.setFont(font); rank.setString("#" + to_string(i + 1));
        rank.setCharacterSize(20); rank.setFillColor(COL_POWER);
        rank.setPosition(WINDOW_W / 2.f - 180.f, y + 10.f); win.draw(rank);
        sf::Text sc; sc.setFont(font);
        sc.setString("Score: " + to_string(scoreboard[i].score));
        sc.setCharacterSize(20); sc.setFillColor(COL_TEXT);
        sc.setPosition(WINDOW_W / 2.f - 60.f, y + 10.f); win.draw(sc);
    }
    if (scoreboardCount == 0) {
        sf::Text em; em.setFont(font); em.setString("No scores yet!");
        em.setCharacterSize(18); em.setFillColor(sf::Color(100, 140, 180));
        auto b = em.getLocalBounds(); em.setOrigin(b.width / 2, 0);
        em.setPosition(WINDOW_W / 2.f, 200); win.draw(em);
    }
    {
        sf::Text t; t.setFont(font); t.setString("Press Esc to return");
        t.setCharacterSize(14); t.setFillColor(sf::Color(80, 120, 170));
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, WINDOW_H - 50.f); win.draw(t);
    }
}

void drawGameOver(sf::RenderWindow& win, const sf::Font& font, int score) {
    win.draw(makeRect(0, 0, WINDOW_W, WINDOW_H, sf::Color(10, 20, 40)));
    auto c = [&](const string& s, float y, sf::Color col, unsigned sz) {
        sf::Text t; t.setFont(font); t.setString(s);
        t.setCharacterSize(sz); t.setFillColor(col);
        auto b = t.getLocalBounds(); t.setOrigin(b.width / 2, 0);
        t.setPosition(WINDOW_W / 2.f, y); win.draw(t);
        };
    c("GAME OVER", 120, sf::Color(220, 60, 60), 48);
    c("Final Score: " + to_string(score), 200, COL_ACCENT, 28);
    c("Score saved to leaderboard!", 260, sf::Color(140, 190, 140), 18);
    c("Press Enter to return to menu", 350, sf::Color(80, 120, 170), 16);
    c("Press Esc to quit", 380, sf::Color(80, 120, 170), 16);
}

// ── MAIN ─────────────────────────────────────────────────────
int main() {
    srand((unsigned)time(0));
    loadScoreboard();

    sf::VideoMode desktop = sf::VideoMode::getDesktopMode();
    sf::RenderWindow window(desktop, "Bite the Bait", sf::Style::Fullscreen);
    window.setFramerateLimit(60);

    // ADD THESE 3 LINES:
    sf::View gameView(sf::FloatRect(0.f, 0.f, (float)WINDOW_W, (float)WINDOW_H));
    gameView.setViewport(sf::FloatRect(0.f, 0.f, 1.f, 1.f));
    window.setView(gameView);
    sf::Font font;
    bool fontOk = false;
    for (auto& p : vector<string>{
            "arial.ttf",
            "C:/Windows/Fonts/arial.ttf",
            "C:/Windows/Fonts/calibri.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" })
            if (font.loadFromFile(p)) { fontOk = true; break; }
    if (!fontOk) { cerr << "Font not found. Copy arial.ttf next to .exe\n"; return 1; }

    Screen screen = Screen::MENU;
    int    menuSel = 0;
    int    finalScore = 0;

    GameState gs;
    gs.init();

    sf::Clock clock;
    float totalTime = 0.f;

    const float PLAYER_SPEED = 270.f; // pixels/sec for smooth glide

    while (window.isOpen()) {
        float dt = clock.restart().asSeconds();
        if (dt > 0.05f) dt = 0.05f;
        totalTime += dt;

        // ── Events ──────────────────────────────────────────
        sf::Event ev;
        while (window.pollEvent(ev)) {
            if (ev.type == sf::Event::Closed) window.close();

            if (screen == Screen::MENU) {
                if (ev.type == sf::Event::KeyPressed) {
                    if (ev.key.code == sf::Keyboard::W || ev.key.code == sf::Keyboard::Up)
                        menuSel = (menuSel + 3) % 4;
                    if (ev.key.code == sf::Keyboard::S || ev.key.code == sf::Keyboard::Down)
                        menuSel = (menuSel + 1) % 4;
                    if (ev.key.code == sf::Keyboard::Return) {
                        if (menuSel == 0) { gs.init(); totalTime = 0; screen = Screen::GAME; }
                        else if (menuSel == 1) screen = Screen::INSTRUCTIONS;
                        else if (menuSel == 2) { loadScoreboard(); screen = Screen::SCOREBOARD; }
                        else window.close();
                    }
                }
            }
            else if (screen == Screen::GAME) {
                if (ev.type == sf::Event::KeyPressed) {
                    if (ev.key.code == sf::Keyboard::Escape) screen = Screen::MENU;

                    if (!gs.player.moving) {
                        int ng = gs.player.gridX, nh = gs.player.gridY;
                        int face = gs.player.facing;
                        if (ev.key.code == sf::Keyboard::W || ev.key.code == sf::Keyboard::Up)
                        {
                            ng--; face = DIR_UP;
                        }
                        else if (ev.key.code == sf::Keyboard::S || ev.key.code == sf::Keyboard::Down)
                        {
                            ng++; face = DIR_DOWN;
                        }
                        else if (ev.key.code == sf::Keyboard::A || ev.key.code == sf::Keyboard::Left)
                        {
                            nh--; face = DIR_LEFT;
                        }
                        else if (ev.key.code == sf::Keyboard::D || ev.key.code == sf::Keyboard::Right)
                        {
                            nh++; face = DIR_RIGHT;
                        }
                        gs.player.facing = face;
                        if (ng >= 0 && ng < GRID_SIZE && nh >= 0 && nh < GRID_SIZE) {
                            gs.player.gridX = ng; gs.player.gridY = nh;
                            gs.player.targetX = (float)nh * CELL_SIZE + CELL_SIZE * 0.5f;
                            gs.player.targetY = (float)ng * CELL_SIZE + CELL_SIZE * 0.5f;
                            gs.player.moving = true;
                        }
                    }
                }
            }
            else {
                if (ev.type == sf::Event::KeyPressed) {
                    if (ev.key.code == sf::Keyboard::Escape || ev.key.code == sf::Keyboard::BackSpace)
                        screen = Screen::MENU;
                    if (screen == Screen::GAMEOVER && ev.key.code == sf::Keyboard::Return)
                        screen = Screen::MENU;
                }
            }
        }

        // ── Update ──────────────────────────────────────────
        if (screen == Screen::GAME) {
            Player& pl = gs.player;
            pl.animAngle += dt * 7.f;

            if (pl.hitTimer > 0.f) pl.hitTimer -= dt;
            // Smooth player glide toward target
            if (pl.moving) {
                float dx = pl.targetX - pl.x, dy = pl.targetY - pl.y;
                float dist = sqrtf(dx * dx + dy * dy);
                float step = PLAYER_SPEED * dt;
                if (step >= dist) { pl.x = pl.targetX; pl.y = pl.targetY; pl.moving = false; }
                else { pl.x += dx / dist * step; pl.y += dy / dist * step; }
            }

            // Spawn timer
            gs.spawnTimer += dt;
            if (gs.spawnTimer > 1.4f) {
                gs.spawnTimer = 0.f;
                if (countAlive(gs.entities, 'F') < MAX_FOOD) {
                    Entity e; spawnEntity(e, 'F'); gs.entities.push_back(e);
                }
                if (countAlive(gs.entities, 'X') < MAX_PRED && rand() % 3 != 0) {
                    Entity e; spawnEntity(e, 'X'); gs.entities.push_back(e);
                }
                if (countAlive(gs.entities, '!') < MAX_POWER && rand() % 6 == 0) {
                    Entity e; spawnEntity(e, '!'); gs.entities.push_back(e);
                }
            }

            // Move entities
            for (auto& e : gs.entities) {
                if (!e.alive) continue;
                e.x += e.vx * dt;
                e.y += e.vy * dt;
                float margin = 45.f;
                if (e.x<-margin || e.x>WORLD_W + margin || e.y<-margin || e.y>WORLD_H + margin)
                    e.alive = false;
            }

            // Player vs entity collision
            float prad = 14.f + (float)(pl.size - 1) * 2.5f;
            for (auto& e : gs.entities) {
                if (!e.alive) continue;
                float er = (e.type == 'X') ? 15.f : 11.f;
                float dx = pl.x - e.x, dy = pl.y - e.y;
                if (sqrtf(dx * dx + dy * dy) < prad + er * 0.72f) {
                    e.alive = false;
                    if (e.type == 'F') { pl.score += 10; pl.size++; }
                    else if (e.type == 'X') {
                        pl.lives--;
                        if (pl.size > 1) pl.size--;
                        pl.hitTimer = 2.0f;   // 2 seconds of blink effect
                    }
                    else if (e.type == '!') { if (pl.lives < MAX_LIVES)pl.lives++; }
                }
            }

            // Remove dead entities
            gs.entities.erase(
                remove_if(gs.entities.begin(), gs.entities.end(),
                    [](const Entity& e) {return !e.alive; }),
                gs.entities.end());

            if (pl.lives <= 0) {
                finalScore = pl.score;
                addScore(finalScore);
                saveScoreboard();
                screen = Screen::GAMEOVER;
            }
        }

        // ── Draw ────────────────────────────────────────────
        window.clear(COL_PANEL);
        switch (screen) {
        case Screen::MENU:
            drawMenu(window, font, menuSel); break;
        case Screen::GAME:
            drawBackground(window, gs, totalTime);

            drawEntities(window, gs.entities, totalTime, gs.player.size);
            drawPlayer(window, gs.player);
            drawHUD(window, gs.player, font);
            break;
        case Screen::INSTRUCTIONS:
            drawInstructions(window, font); break;
        case Screen::SCOREBOARD:
            drawScoreboard(window, font); break;
        case Screen::GAMEOVER:
            drawGameOver(window, font, finalScore); break;
        }
        window.display();
    }
    return 0;
}