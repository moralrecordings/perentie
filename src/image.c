#include <stdio.h>
#include <stdlib.h>

#include "stb/stb_image.h"


void get_image(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        printf("Failed to open image: %s", path);
        return;
    }

    int width;
    int height;
    int channels;
    if (!stbi_info_from_file(fp, &width, &height, &channels)) {
        printf("Unable to parse image: %s", stbi_failure_reason());
        return;
    }

    fclose(fp);
}
