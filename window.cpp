#include "window.h"

void Window::draw(Drawer &drawer) {
    Color outline = Color(37, 112, 208, 255);
    Color titleColor = Color(239, 237, 224, 255);
    // Window
    drawer.drawRect(x, y, x+w, y+h, bg);

    // Outline
    drawer.drawLine(x, y, x+w, y, outline);
    drawer.drawLine(x, y, x, y+h, outline);
    drawer.drawLine(x+w, y, x+w, y+h, outline);
    drawer.drawLine(x, y+h, x+w, y+h, outline);

    // Title
    int winTitleH = 16;
    drawer.drawRect(x, y-winTitleH, x+w, y, outline);
}