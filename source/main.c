#include <switch.h>
#include <stdio.h>
#include <stdint.h>
#include "game.h"

#define SAVE_PATH "sdmc:/switch/pixeldash.sav"
#define SCALE 4   /* 320x180 -> 1280x720 */

static uint32_t pixels[GW * GH];

static int load_hiscore(void) {
    int v = 0;
    FILE *f = fopen(SAVE_PATH, "rb");
    if (f) { if (fread(&v, sizeof v, 1, f) != 1) v = 0; fclose(f); }
    return v < 0 ? 0 : v;
}
static void save_hiscore(int v) {
    FILE *f = fopen(SAVE_PATH, "wb");
    if (f) { fwrite(&v, sizeof v, 1, f); fclose(f); }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    Framebuffer fb;
    framebufferCreate(&fb, nwindowGetDefault(), 1280, 720, PIXEL_FORMAT_RGBA_8888, 2);
    framebufferMakeLinear(&fb);

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);

    Game game;
    game_init(&game, load_hiscore());
    int saved_hi = game.hiscore;

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        u64 held = padGetButtons(&pad);
        if (down & HidNpadButton_Plus) break;

        const u64 JUMP = HidNpadButton_A | HidNpadButton_B | HidNpadButton_X | HidNpadButton_Y | HidNpadButton_Up;
        int in = 0;
        if (down & JUMP) in |= IN_JUMP_PRESSED;
        if (held & JUMP) in |= IN_JUMP_HELD;

        game_update(&game, in);
        if (game.state == 2 && game.hiscore != saved_hi) { save_hiscore(game.hiscore); saved_hi = game.hiscore; }
        game_render(&game, pixels);

        u32 stride;
        u8 *out = (u8 *)framebufferBegin(&fb, &stride);
        for (int y = 0; y < 720; y++) {
            u32 *row = (u32 *)(out + (size_t)y * stride);
            const uint32_t *src = &pixels[(y / SCALE) * GW];
            for (int x = 0; x < 1280; x++) row[x] = src[x / SCALE];
        }
        framebufferEnd(&fb);   /* ждёт vsync -> стабильные 60 FPS */
    }

    framebufferClose(&fb);
    return 0;
}
