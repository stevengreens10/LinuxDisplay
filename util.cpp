#include "util.h"
#include "drm.h"
#include <cmath>
#include <cerrno>
#include <time.h>

/*
    Linearly interpolates from x1 to x2 as t goes from 0 to 1
*/
double Util::lerp(double x1, double x2, double t) {
    return (x2 - x1) * t + x1;
}

Color Util::lerpColor(Color p1, Color p2, double t) {
    Color p;
    p.r = floor(lerp(p1.r, p2.r, t));
    p.g = floor(lerp(p1.g, p2.g, t));
    p.b = floor(lerp(p1.b, p2.b, t));
    p.a = floor(lerp(p1.a, p2.a, t));
    return p;
}

/* msleep(): Sleep for the requested number of milliseconds. */
int Util::msleep(double msec)
{
    timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = floor(msec / 1000);
    double remainder = msec - ts.tv_sec * 1000;
    ts.tv_nsec = floor(remainder * 1000000);

    do {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

int Util::screenWidth() {
    return DRM::inst()->currentFramebuffer().width;
}

int Util::screenHeight() {
    return DRM::inst()->currentFramebuffer().height;
}