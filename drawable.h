#ifndef DRAWABLE_H
#define DRAWABLE_H

#include "draw.h"

class Drawable {
public:
    virtual void draw(Drawer &drawer) = 0;
};

#endif