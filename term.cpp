#include "term.h"
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <linux/kd.h>
#include <linux/vt.h>
#include <sys/ioctl.h>

int Term::setGraphicsMode() {
    tty = open("/dev/tty", O_RDWR);
    if (tty < 0) {
        printf("Error opening terminal");
        return 1;
    }
    
    // Save current terminal attributes
    tcgetattr(0, &orig_termios);

    // Turn on raw mode
    termios new_termios = orig_termios;
    cfmakeraw(&new_termios);
    // Add back out processing
    new_termios.c_oflag |= (OPOST|ONLCR);
    tcsetattr(0, TCSANOW, &new_termios);


    // Save current terminal mode
    ioctl(tty, VT_GETMODE, &termMode);

    // Set terminal to graphics mode
    vt_mode new_mode = { .mode = VT_PROCESS };
    ioctl(tty, VT_SETMODE, &new_mode);

    ioctl(tty, KDSETMODE, KD_GRAPHICS);

    return 0;
}

void Term::setTextMode() {
    // Restore terminal mode
    tcsetattr(0, TCSANOW, &orig_termios);

    ioctl(tty, KDSETMODE, KD_TEXT);
    ioctl(tty, VT_SETMODE, &termMode);

    close(tty);
}