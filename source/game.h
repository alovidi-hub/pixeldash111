#pragma once
#include <stdint.h>

#define GW 320   /* внутреннее разрешение, как у Mega Drive (1280x720 / 4) */
#define GH 180

#define IN_JUMP_PRESSED 1
#define IN_JUMP_HELD    2

#define MAX_ENT 24

typedef struct { int type; float x, y; int w, h; int active; } Ent;

typedef struct {
    int state;                 /* 0 - титул, 1 - игра, 2 - проигрыш */
    float feet, vy;            /* позиция ног и скорость героя */
    int onground;
    float speed, scroll, dist, spawn_timer;
    int score, gems, hiscore, frame, dead_frame;
    uint32_t rng;
    Ent ent[MAX_ENT];
} Game;

void game_init(Game *g, int hiscore);
void game_update(Game *g, int input);
void game_render(const Game *g, uint32_t *pixels); /* GW*GH, байты R,G,B,A */
