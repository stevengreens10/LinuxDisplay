#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "util.h"

class FrameBuffer {
    public:
    int width, height, size, pitch, bpp;

    Color *buf;
};

#endif