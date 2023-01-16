#ifndef DRAW_H
#define DRAW_H

#include "util.h"
#include "framebuffer.h"

class Drawer {
private:
    FrameBuffer &frameBuffer;
public:

    Drawer(FrameBuffer &fb) : frameBuffer(fb) {}

    void setFrameBuffer(FrameBuffer &frameBuffer);

    Color getPixel(int x, int y);
    void drawPixel(int x, int y, Color c);
    void drawRect(int x1, int y1, int x2, int y2, Color color);
    void drawLine(int x1, int y1, int x2, int y2, Color color);
    void drawCircle(int xc, int yc, int radius, Color color);
    void drawFilledCircle(int x0, int y0, int radius, Color color);
    void drawSinWave(double amplitude, double frequency, double offset,  Color color);
};

#endif