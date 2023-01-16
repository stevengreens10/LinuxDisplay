#ifndef UTIL_H
#define UTIL_H

class Color {
public:
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
    
    Color(unsigned char r, unsigned char g, unsigned char b, unsigned char a) :
        r(r), g(g), b(b), a(a) {}

    Color() : Color(0, 0, 0, 0) {}
};

namespace Util {
    double lerp(double x1, double x2, double t);

    Color lerpColor(Color p1, Color p2, double t);

    /* msleep(): Sleep for the requested number of milliseconds. */
    int msleep(double msec);

    int screenWidth();
    int screenHeight();
}

#endif