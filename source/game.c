#include "game.h"
#include <stdio.h>
#include <string.h>

/* Палитра в духе Mega Drive: по 3 бита на канал (0..7) */
#define C(r,g,b) (0xFF000000u | ((uint32_t)((b)*255/7)<<16) | ((uint32_t)((g)*255/7)<<8) | (uint32_t)((r)*255/7))

#define GROUND 140
#define PX 56
#define PW 16
#define PH 16
enum { E_SPIKE, E_CRATE, E_GEM };

/* ---------- ГЕНЕРАТОР СЛУЧАЙНЫХ ЧИСЕЛ ---------- */
static uint32_t rnd(Game *g) {
    uint32_t x = g->rng;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return g->rng = x;
}

/* ---------- ЛОГИКА ---------- */
static void add_ent(Game *g, int type, float x, float y, int w, int h) {
    for (int i = 0; i < MAX_ENT; i++) {
        if (!g->ent[i].active) {
            Ent e = { type, x, y, w, h, 1 };
            g->ent[i] = e;
            return;
        }
    }
}

static void gem_arc(Game *g, float cx) {
    add_ent(g, E_GEM, cx - 27, GROUND - 44, 10, 10);
    add_ent(g, E_GEM, cx - 5,  GROUND - 58, 10, 10);
    add_ent(g, E_GEM, cx + 17, GROUND - 44, 10, 10);
}

static void spawn(Game *g) {
    float x = GW + 16;
    int r = rnd(g) % 10;
    float cx;
    if (r <= 3) {
        add_ent(g, E_SPIKE, x, GROUND - 14, 16, 14); cx = x + 8;
    } else if (r <= 5) {
        add_ent(g, E_SPIKE, x, GROUND - 14, 16, 14);
        add_ent(g, E_SPIKE, x + 16, GROUND - 14, 16, 14); cx = x + 16;
    } else if (r <= 7) {
        add_ent(g, E_CRATE, x, GROUND - 18, 18, 18); cx = x + 9;
    } else {
        for (int i = 0; i < 4; i++) add_ent(g, E_GEM, x + i * 18, GROUND - 14, 10, 10);
        return;
    }
    if (rnd(g) % 2) gem_arc(g, cx);
}

static int overlap(float al, float at, float ar, float ab, float bl, float bt, float br, float bb) {
    return ar > bl && al < br && ab > bt && at < bb;
}

static void reset_run(Game *g) {
    int hi = g->hiscore;
    uint32_t seed = g->rng ? g->rng : 0x1234ABCDu;
    memset(g, 0, sizeof(*g));
    g->hiscore = hi; g->rng = seed;
    g->feet = GROUND; g->onground = 1;
    g->speed = 2.4f; g->spawn_timer = 60;
}

void game_init(Game *g, int hiscore) {
    memset(g, 0, sizeof(*g));
    g->hiscore = hiscore;
    g->rng = 0x1234ABCDu;
    reset_run(g);
    g->state = 0;
}

void game_update(Game *g, int in) {
    int jp = in & IN_JUMP_PRESSED, jh = in & IN_JUMP_HELD;
    g->frame++;
    g->rng ^= (uint32_t)g->frame * 2654435761u; /* немного энтропии от тайминга */
    if (!g->rng) g->rng = 1;

    if (g->state == 0) {
        g->scroll += 1.5f;
        if (jp) { reset_run(g); g->state = 1; }
        return;
    }

    if (g->state == 2) {
        g->dead_frame++;
        g->feet += g->vy; g->vy += 0.30f;
        if (g->dead_frame > 30 && jp) { reset_run(g); g->state = 1; }
        return;
    }

    /* ---- игра ---- */
    if (g->speed < 5.0f) g->speed += 0.0007f;
    g->scroll += g->speed;
    g->dist += g->speed;

    g->spawn_timer -= g->speed;
    if (g->spawn_timer <= 0) {
        spawn(g);
        g->spawn_timer = 100 + g->speed * 18 + (rnd(g) % 110);
    }
    for (int i = 0; i < MAX_ENT; i++) {
        if (!g->ent[i].active) continue;
        g->ent[i].x -= g->speed;
        if (g->ent[i].x + g->ent[i].w < -20) g->ent[i].active = 0;
    }

    float prev = g->feet;
    if (jp && g->onground) { g->vy = -5.4f; g->onground = 0; }
    if (!jh && g->vy < -2.0f) g->vy = -2.0f;      /* короткий прыжок, если отпустить кнопку */
    g->vy += 0.30f; g->feet += g->vy;
    g->onground = 0;
    if (g->feet >= GROUND) { g->feet = GROUND; g->vy = 0; g->onground = 1; }

    float pl = PX + 3, pr = PX + PW - 3;
    for (int i = 0; i < MAX_ENT; i++) {            /* приземление на ящик */
        Ent *e = &g->ent[i];
        if (!e->active || e->type != E_CRATE) continue;
        if (pr > e->x && pl < e->x + e->w && g->vy >= 0 && prev <= e->y + 1 && g->feet >= e->y) {
            g->feet = e->y; g->vy = 0; g->onground = 1;
        }
    }

    float pt = g->feet - PH + 3, pb = g->feet - 1;
    for (int i = 0; i < MAX_ENT; i++) {
        Ent *e = &g->ent[i];
        if (!e->active) continue;
        if (e->type == E_GEM) {
            if (overlap(pl, pt, pr, pb, e->x, e->y, e->x + e->w, e->y + e->h)) { e->active = 0; g->gems++; }
        } else {
            float l = e->x, t = e->y, r = e->x + e->w, b = e->y + e->h;
            if (e->type == E_SPIKE) { l += 3; r -= 3; t += 4; }
            if (overlap(pl, pt, pr, pb, l, t, r, b)) {
                g->state = 2; g->dead_frame = 0; g->vy = -5.0f;
                g->score = (int)(g->dist / 8) + g->gems * 10;
                if (g->score > g->hiscore) g->hiscore = g->score;
                return;
            }
        }
    }
    g->score = (int)(g->dist / 8) + g->gems * 10;
    if (g->score > g->hiscore) g->hiscore = g->score;
}

/* ---------- ГРАФИКА ---------- */
static void put(uint32_t *b, int x, int y, uint32_t c) {
    if ((unsigned)x < GW && (unsigned)y < GH) b[y * GW + x] = c;
}
static void rect(uint32_t *b, int x, int y, int w, int h, uint32_t c) {
    for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) put(b, x + i, y + j, c);
}

/* Шрифт 3x5: цифры 0-9, затем буквы A-Z (по 5 строк по 3 бита) */
static const char *FONT[36] = {
 "111101101101111","010110010010111","110001010100111","110001010001110","101101111001001",
 "111100110001110","011100111101111","111001010010010","111101111101111","111101111001110",
 "010101111101101","110101110101110","011100100100011","110101101101110","111100110100111",
 "111100110100100","011100101101011","101101111101101","111010010010111","001001001101010",
 "101101110101101","100100100100111","101111111101101","110101101101101","010101101101010",
 "110101110100100","010101101111011","110101110101101","011100010001110","111010010010010",
 "101101101101111","101101101101010","101101111111101","101101010101101","101101010010010",
 "111001010100111"
};

static int text_w(const char *s, int sc) { return (int)strlen(s) * 4 * sc - sc; }
static void text1(uint32_t *b, const char *s, int x, int y, int sc, uint32_t col) {
    for (; *s; s++, x += 4 * sc) {
        int idx = -1;
        if (*s >= '0' && *s <= '9') idx = *s - '0';
        else if (*s >= 'A' && *s <= 'Z') idx = 10 + *s - 'A';
        if (idx < 0) continue;
        for (int j = 0; j < 5; j++) for (int i = 0; i < 3; i++)
            if (FONT[idx][j * 3 + i] == '1') rect(b, x + i * sc, y + j * sc, sc, sc, col);
    }
}
static void text(uint32_t *b, const char *s, int x, int y, int sc, uint32_t col, uint32_t sh) {
    int o = sc > 2 ? 2 : sc;
    text1(b, s, x + o, y + o, sc, sh);
    text1(b, s, x, y, sc, col);
}
static void text_c(uint32_t *b, const char *s, int y, int sc, uint32_t col, uint32_t sh) {
    text(b, s, (GW - text_w(s, sc)) / 2, y, sc, col, sh);
}

/* Спрайт героя 16x16 (смотрит вправо) */
static const char *HERO_TOP[12] = {
 "....kkkkkkk.....",
 "...kbbbbbbbbk...",
 "..kbBBbbbbbbbk..",
 "..kbBbbkkkkkwk..",
 "..kbbbkwwkkkwk..",
 "..kbbbbkkkkkkk..",
 "..kbbbbbbbbbk...",
 "...kkbbbbbbkk...",
 "rrrrkbbwwbbbk...",
 ".rrrkbwwwwbbk...",
 "..rrkbbwwbbk....",
 "....kbbbbbbk....",
};
static const char *LEGS[3][4] = {
 { "....kbbkkbbk....", "...kbbk..kbbk...", "..kssk....kssk..", "..kkk......kkk.." },
 { ".....kbbbbk.....", ".....kbbbbk.....", "....kssssssk....", "....kkkkkkkk...." },
 { "...kbbkkkkbbk...", "..kssk....kssk..", "..kkk......kkk..", "................" },
};

static uint32_t hero_col(char c) {
    switch (c) {
        case 'k': return C(0,0,2);
        case 'b': return C(1,3,7);
        case 'B': return C(4,6,7);
        case 'w': return C(7,7,7);
        case 'r': return C(7,1,1);
        case 's': return C(7,2,0);
    }
    return 0;
}

static void draw_hero(uint32_t *b, int x, int y, int legs, int flip) {
    for (int j = 0; j < 16; j++) {
        const char *row = j < 12 ? HERO_TOP[j] : LEGS[legs][j - 12];
        int yy = flip ? y + 15 - j : y + j;
        for (int i = 0; i < 16; i++) {
            uint32_t c = hero_col(row[i]);
            if (c) put(b, x + i, yy, c);
        }
    }
}

static int tri(int x, int P) { int m = x % P; if (m < 0) m += P; int h = P / 2; return m < h ? m : P - m; }

static void draw_coin(uint32_t *b, int x, int y, int frame) {
    static const int rxs[4] = { 5, 3, 1, 3 };
    int rx = rxs[(frame / 6) & 3];
    for (int j = 0; j < 10; j++) for (int i = 0; i < 10; i++) {
        float dx = (i - 4.5f) / (rx + 0.5f), dy = (j - 4.5f) / 5.0f;
        float d = dx * dx + dy * dy;
        if (d <= 1.0f) put(b, x + i, y + j, d > 0.65f ? C(5,2,0) : C(7,6,0));
    }
    if (rx >= 3) { put(b, x + 4, y + 2, C(7,7,5)); put(b, x + 4, y + 3, C(7,7,5)); }
}

static void draw_bg(const Game *g, uint32_t *b) {
    static const uint32_t SKY[6] = { C(0,1,5), C(0,2,5), C(1,3,6), C(1,4,6), C(2,5,7), C(3,6,7) };
    int sc = (int)g->scroll;
    for (int y = 0; y < GH; y++) {
        int band = y / 24; if (band > 5) band = 5;
        int o = y % 24;
        for (int x = 0; x < GW; x++) {
            uint32_t c = SKY[band];
            if (o >= 20 && band < 5 && ((x + y) & 1)) c = SKY[band + 1];
            b[y * GW + x] = c;
        }
    }
    /* облака */
    for (int i = 0; i < 3; i++) {
        int cx = (i * 110 - (int)(sc * 0.15f)) % 330; if (cx < 0) cx += 330; cx -= 40;
        int cy = 18 + (i * 37) % 40;
        rect(b, cx + 4, cy + 4, 26, 6, C(7,7,7));
        rect(b, cx + 9, cy, 12, 5, C(7,7,7));
        rect(b, cx + 4, cy + 9, 26, 1, C(5,6,7));
    }
    /* далёкие горы */
    for (int x = 0; x < GW; x++) {
        int wx = x + (int)(sc * 0.3f);
        int h = 26 + tri(wx, 120) * 34 / 60 + tri(wx, 34) * 6 / 17;
        for (int y = GROUND - h; y < GROUND; y++) {
            uint32_t c = C(3,2,5);
            if (y - (GROUND - h) < 2) c = (h > 55) ? C(7,7,7) : C(5,4,6);
            else if (((x >> 2) + (y >> 2)) & 1) c = C(3,2,4);
            put(b, x, y, c);
        }
    }
    /* ближние холмы с шахматной раскраской */
    for (int x = 0; x < GW; x++) {
        int wx = x + (int)(sc * 0.6f);
        int h = 10 + tri(wx, 150) * 26 / 75;
        for (int y = GROUND - h; y < GROUND; y++) {
            uint32_t c = (((wx >> 3) + (y >> 3)) & 1) ? C(1,5,2) : C(1,4,2);
            if (y - (GROUND - h) < 2) c = C(3,7,3);
            put(b, x, y, c);
        }
    }
    /* земля */
    for (int y = GROUND; y < GH; y++) for (int x = 0; x < GW; x++) {
        uint32_t c;
        int dy = y - GROUND;
        if (dy < 2) c = C(4,7,3);
        else if (dy < 6) c = C(1,5,1);
        else if (dy == 6) c = C(2,1,0);
        else c = ((((x + sc) >> 4) + (dy >> 4)) & 1) ? C(5,3,1) : C(4,2,1);
        b[y * GW + x] = c;
    }
}

static void draw_ent(uint32_t *b, const Ent *e, int frame) {
    int x = (int)e->x, y = (int)e->y;
    if (e->type == E_SPIKE) {
        for (int r = 0; r < e->h; r++) {
            int wd = 2 + r * (e->w - 2) / (e->h - 1);
            int x0 = x + (e->w - wd) / 2;
            for (int i = 0; i < wd; i++) {
                uint32_t c = (i < wd / 2) ? C(6,6,7) : C(3,3,5);
                if (i == 0 || i == wd - 1) c = C(1,1,2);
                put(b, x0 + i, y + r, c);
            }
        }
    } else if (e->type == E_CRATE) {
        rect(b, x, y, e->w, e->h, C(2,1,0));
        rect(b, x + 1, y + 1, e->w - 2, e->h - 2, C(5,3,1));
        rect(b, x + 1, y + 1, e->w - 2, 1, C(6,4,2));
        for (int i = 2; i < e->w - 2; i++) {
            put(b, x + i, y + 2 + (i - 2) * (e->h - 5) / (e->w - 5), C(3,2,1));
            put(b, x + i, y + e->h - 3 - (i - 2) * (e->h - 5) / (e->w - 5), C(3,2,1));
        }
    } else {
        draw_coin(b, x, y, frame);
    }
}

void game_render(const Game *g, uint32_t *b) {
    char buf[32];
    draw_bg(g, b);

    for (int i = 0; i < MAX_ENT; i++) if (g->ent[i].active) draw_ent(b, &g->ent[i], g->frame);

    int legs = g->onground ? (g->frame / 5) & 1 : 2;
    if (g->state == 2) draw_hero(b, PX, (int)g->feet - 16, 2, 1);
    else draw_hero(b, PX, (int)g->feet - 16, legs, 0);

    uint32_t W = C(7,7,7), D = C(0,0,2);
    if (g->state == 1 || g->state == 2) {
        snprintf(buf, sizeof buf, "SCORE %d", g->score);
        text(b, buf, 8, 8, 2, W, D);
        snprintf(buf, sizeof buf, "BEST %d", g->hiscore);
        text(b, buf, GW - 8 - text_w(buf, 2), 8, 2, C(7,6,0), D);
        draw_coin(b, 8, 24, 0);
        snprintf(buf, sizeof buf, "X%d", g->gems);
        text(b, buf, 22, 26, 2, W, D);
    }
    if (g->state == 0) {
        int bob = (int)((g->frame / 8) % 2);
        text_c(b, "PIXEL DASH", 30 + bob, 5, C(7,6,0), C(6,1,0));
        text_c(b, "A TINY RUNNER FOR SWITCH", 66, 1, W, D);
        if ((g->frame / 30) & 1) text_c(b, "PRESS A TO START", 100, 2, W, D);
        snprintf(buf, sizeof buf, "BEST %d", g->hiscore);
        text_c(b, buf, 156, 1, C(7,6,0), D);
        text_c(b, "A JUMP  HOLD A FOR HIGHER  PLUS QUIT", 168, 1, W, D);
    }
    if (g->state == 2) {
        for (int y = 0; y < GH; y++) for (int x = 0; x < GW; x++)
            if ((x + y) & 1) b[y * GW + x] = C(0,0,1);
        text_c(b, "GAME OVER", 50, 5, C(7,2,1), D);
        snprintf(buf, sizeof buf, "SCORE %d", g->score);
        text_c(b, buf, 92, 2, W, D);
        snprintf(buf, sizeof buf, "BEST %d", g->hiscore);
        text_c(b, buf, 112, 2, C(7,6,0), D);
        if (g->dead_frame > 30 && ((g->frame / 30) & 1)) text_c(b, "PRESS A", 140, 2, W, D);
    }
}
