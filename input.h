#ifndef INPUT_H
#define INPUT__H

#include <unistd.h>
#include <memory>
#include <cstdio>
#include <functional>

typedef struct MouseData {
    int x, y = 0;
    int leftBtn, rightBtn = 0;
} MouseData;

typedef struct KeyData {
    unsigned char keys[256] = {};
} KeyData;

typedef std::function<void(const MouseData&)> MouseCallback;
typedef std::function<void(const KeyData&)> KeyCallback;

class Input {
private:
    int mouse_fd = 0;
    int key_fd = 0;
    MouseCallback mouseCallback;
    KeyCallback keyCallback;

    inline static std::unique_ptr<Input> instance{nullptr};
    Input() = default;

public:

    MouseData mouseData;
    KeyData keyData;

    static Input* inst();

    ~Input() {
        printf("Closing input files\n");
        close(mouse_fd);
        close(key_fd);
    }

    Input(Input &) = delete;
    void operator=(const Input &) = delete;
    
    void setMouseCallback(MouseCallback cb) {
        mouseCallback = cb;
    }

    void setKeyCallback(KeyCallback cb) {
        keyCallback = cb;
    }

    int getInput();
};

#endif