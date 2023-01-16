#ifndef DRM_H
#define DRM_H

#include <drm.h>
#include "framebuffer.h"
#include <memory>
#include <cstdio>
#include <functional>

typedef std::function<void(void*)> SwapCallback;

typedef struct FrameBufferData {
    FrameBuffer frameBuf;
    int fb_id;
    int crtc_id;
    int conn_id;
    int handle;
} FrameBufferData;

class DRM {
protected:
    
    inline static std::unique_ptr<DRM> instance{nullptr};
    // Protected constructor
    DRM() {}

private:

    SwapCallback swapCallback;
    void *swapParam;

    // Keeps track of internal data related to framebuffers
    FrameBufferData fbData[2];

    int drm_fd;
    // Current buffer that should be drawn to (0 or 1)
    int currentBuf = 0;
    
    // 0 for original, 1 for framebuffer
    int crtcMode;
    drm_mode_crtc old_crtc;
    drm_mode_crtc new_crtc;

public:

    // RAII: Clean framebuffers on destruction
    ~DRM() {
        printf("Clean framebuffers\n");
        cleanFramebuffers();
    }

    // Delete copy constructor
    DRM(DRM &o) = delete;
    // Prevent assignment
    void operator=(const DRM &) = delete;

    static DRM* inst();

    FrameBuffer &currentFramebuffer() {
        return fbData[currentBuf].frameBuf;
    }

    void setSwapCallback(SwapCallback cb, void *userData);
    void swapBuffers();
    int setupFramebuffers();
    int handleEvents();
    int cleanFramebuffers();

    int setCRTC(int mode);
    void swapMode();

    void onVBlankEvent(drm_event_vblank *vblank);
    void onPageFlipCompleteEvent(drm_event_vblank *vblank);
    void onSequenceEvent(drm_event_crtc_sequence *seq);
};

#endif