#ifndef WINDOW_H
#define WINDOW_H

#include "drawable.h"

class Window : Drawable {
public:
    int x, y, w, h = 0;
    Color bg = Color(255, 255, 255, 255);

    Window(int x, int y, int w, int h) : 
        x(x), y(y), w(w), h(h) {}

    void draw(Drawer &drawer);

};

#endif