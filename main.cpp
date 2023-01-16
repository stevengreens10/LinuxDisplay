#include <cstdio>
#include "drm.h"
#include "draw.h"
#include "term.h"
#include "window.h"
#include "input.h"
#include "util.h"
#include <linux/input.h>
#include <thread>

Window *window;

void drawLoop(Drawer *drawer) {
    drawer->drawRect(0, 0, Util::screenWidth(), Util::screenHeight(), Color(151, 199, 207, 255));
    
    window->draw(*drawer);

    auto mouseData = Input::inst()->mouseData;
    drawer->drawRect(mouseData.x, mouseData.y, mouseData.x + 16, mouseData.y + 16, Color(255, 0, 0, 255));

    DRM::inst()->swapBuffers();
}

int main() {

    if(Term::setGraphicsMode() > 0) {
        printf("Could not set graphics mode");
        return 1;
    }
    
    if(DRM::inst()->setupFramebuffers() > 0) {
        printf("Error: Could not setup framebuffer\n");
        return 1;
    }

    Drawer *drawer = new Drawer(DRM::inst()->currentFramebuffer());
    DRM *drm = DRM::inst();


    int width = 640;
    int height = 480;
    window = new Window(Util::screenWidth() / 2 - width/2, Util::screenHeight() / 2 - height/2, 640, 480);

    drm->setSwapCallback([](void *userData) {
        Drawer *drawer = (Drawer *) userData;
        drawer->setFrameBuffer(DRM::inst()->currentFramebuffer());
        drawLoop(drawer);
    }, drawer);

    drawLoop(drawer);
    Input *input = Input::inst();

    bool running = true;

    input->setMouseCallback([&](const MouseData &md) {
        //printf("Mouse callback: (%d, %d) [%d|%d]\n", md.x, md.y, md.leftBtn, md.rightBtn);
        if(md.leftBtn) {
            window->x = md.x;
            window->y = md.y;
        } else if(md.rightBtn) {
            running = false;
        }
    });

    input->setKeyCallback([&](const KeyData &kd) {
        printf("Key callback!\n");
        if(kd.keys[KEY_H] > 0) {
            printf("Swapping display mode\n");
            drm->swapMode();
        }
        if(kd.keys[KEY_P] > 0) {
            drm->swapBuffers();
        }
        if(kd.keys[KEY_Q] > 0) {
            printf("Exiting!\n");
            running = false;
        }
    });

    // Handle DRM events on a separate thread.
    // This will also call the swap callback so this thread
    // will do all the drawing
    std::thread drmThread([&] {
        while (running) {
            drm->handleEvents();
        }
    });

    // Event loop
    while(running) {
        input->getInput();
    }

    drmThread.join();

    delete window;
    Term::setTextMode();
    
    return 0;
}
