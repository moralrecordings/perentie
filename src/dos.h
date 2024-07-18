#ifndef PERENTIE_DOS_H
#define PERENTIE_DOS_H

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

struct image;

void video_init();
void video_test(int offset);
void video_blit_image(struct image *image, int16_t x, int16_t y);
void video_shutdown();

#endif
