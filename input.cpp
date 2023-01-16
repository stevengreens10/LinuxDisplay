#include "input.h"
#include <fcntl.h>
#include <cstdio>
#include <poll.h>
#include <unistd.h>
#include "util.h"
#include <linux/input.h>
#include <cmath>

Input* Input::inst() {
    if(!instance) {
        instance = std::unique_ptr<Input>(new Input());
    }
    return instance.get();
}

std::vector<input_event> getEvents(int fd) {
    std::vector<input_event> events;
    bool moreEvents;
    do {
        input_event nextEvent;
        ssize_t bytesRead = read(fd, &nextEvent, sizeof(struct input_event));
        if(bytesRead < sizeof(input_event)) break;

        events.push_back(nextEvent);

        pollfd fds[] = {{.fd = fd, .events = POLLIN}};
        int ret = poll(fds, 1, 0);
        if(ret < 0) break;

        moreEvents = fds[0].revents & POLLIN;
    } while(moreEvents);
    return events;
}

int Input::getInput() {
    if(mouse_fd == 0) {
        mouse_fd = open("/dev/input/event3", O_RDONLY);
        if(mouse_fd < 0) {
            printf("Could not open mouse button file\n");
            return 1;
        }
    }

    if(key_fd == 0) {
        key_fd = open("/dev/input/event2", O_RDONLY);
        if(key_fd < 0) {
            printf("Could not open key file\n");
            return 1;
        }
    }

    pollfd fds[] = {
        {.fd = key_fd, .events = POLLIN},
        {.fd = mouse_fd, .events = POLLIN},
    };
    
    if(poll(fds, 2, 1) < 0) {
        printf("Error on input poll\n");
        return 1;
    }

    bool keyInput = false;
    bool mouseInput = false;

    // KEY INPUT
    if(fds[0].revents & POLLIN) {
        printf("Reading key events\n");
        auto events = getEvents(key_fd);

        for(const auto &event : events) {
            if(event.type != EV_KEY) continue;
            printf("KEY event %d = %d\n", event.code, event.value);
            keyData.keys[event.code & 0xff] = event.value & 0xff;
        }

        keyInput = true;
    }

    // MOUSE INPUT
    if(fds[1].revents & POLLIN) {
        auto events = getEvents(mouse_fd);
        
        for(const auto &event : events) {
            // ABS coordinates are from 0 to 32768
            if(event.type == EV_ABS) {
                if(event.code == ABS_X) {
                    mouseData.x = floor(((double) (event.value) / 32768) * Util::screenWidth());
                } else if(event.code == ABS_Y) {
                    mouseData.y = floor(((double) (event.value) / 32768) * Util::screenHeight());
                }
            } else if(event.type == EV_KEY) {
                if(event.code == BTN_LEFT) {
                    mouseData.leftBtn = event.value & 0xff;
                } else if(event.code == BTN_RIGHT) {
                    mouseData.rightBtn = event.value & 0xff;
                }
            }
        }

        mouseInput = true;
    }

    if(keyInput)
        keyCallback(keyData);
    if(mouseInput)
        mouseCallback(mouseData);

    return 0;
}