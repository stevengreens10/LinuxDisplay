#ifndef TERM_H
#define TERM_H

#include <termios.h>

// TODO: make class
namespace Term {
    static termios orig_termios;
    static int tty;
    static int termMode;
    int setGraphicsMode();
    void setTextMode();
}

#endif