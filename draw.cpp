#include "draw.h"
#include "drm.h"

#include <cstdio>
#define _USE_MATH_DEFINES
#include <cmath>

void Drawer::setFrameBuffer(FrameBuffer &frameBuffer) {
    this->frameBuffer = frameBuffer;
}

Color Drawer::getPixel(int x, int y) {
	Color c = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= frameBuffer.width || y >= frameBuffer.height) {
        return c;
    }

	int index = y * (frameBuffer.pitch / frameBuffer.bpp) + x;
	if(index * 4 >= frameBuffer.size) {
		printf("Tried to get a pixel out of bounds! Index %d\n", index);
	}
	return frameBuffer.buf[index];
}

void Drawer::drawPixel(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= frameBuffer.width || y >= frameBuffer.height) {
        return;
    }
    int index = y * frameBuffer.width + x;
	if(index * 4 >= frameBuffer.size) {
		printf("Tried to draw a pixel out of bounds! Index %d\n", index);
	}
   frameBuffer.buf[index] = color;
}

void Drawer::drawRect(int x1, int y1, int x2, int y2, Color color) {
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            drawPixel(x, y, color);
        }
    }
}

void Drawer::drawLine(int x1, int y1, int x2, int y2, Color color) {
    // Implement Bresenham's line drawing algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        drawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void Drawer::drawCircle(int xc, int yc, int radius, Color color) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    while (y >= x) {
        drawPixel(xc + x, yc + y, color);
        drawPixel(xc - x, yc + y, color);
        drawPixel(xc + x, yc - y, color);
        drawPixel(xc - x, yc - y, color);
        drawPixel(xc + y, yc + x, color);
        drawPixel(xc - y, yc + x, color);
        drawPixel(xc + y, yc - x, color);
        drawPixel(xc - y, yc - x, color);

        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void Drawer::drawFilledCircle(int x0, int y0, int radius, Color color) {
    int x = radius;
    int y = 0;
    int radiusError = 1-x;

    while(x >= y) {
        drawLine(x0 + x, y0 + y, x0 - x, y0 + y, color);
        drawLine(x0 + y, y0 + x, x0 - y, y0 + x, color);
        drawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        drawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
        y++;
        if (radiusError<0) {
            radiusError += 2 * y + 1;
        } else {
            x--;
            radiusError += 2 * (y - x + 1);
        }
    }
}

void Drawer::drawSinWave(double amplitude, double frequency, double offset,  Color color) {
    double x1, y1, x2, y2;
    for (int x = 0; x < frameBuffer.width; x++) {
        x1 = x;
        y1 = amplitude * sin((2 * M_PI * frequency * x / frameBuffer.width)+offset) + frameBuffer.height/2;
        x2 = x+1;
        y2 = amplitude * sin((2 * M_PI * frequency * (x+1) / frameBuffer.width)+offset) + frameBuffer.height/2;
        drawLine(x1, y1, x2, y2, color);
    }
}
