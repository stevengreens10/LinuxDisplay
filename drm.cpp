#include <cstdio>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <drm.h>
#include <cstdlib>
#include <cstdint>
#include <poll.h>
#include "drm.h"

DRM* DRM::inst() {
    if(!instance) {
        instance = std::unique_ptr<DRM>(new DRM());
    }
    return instance.get();
}

void DRM::setSwapCallback(SwapCallback cb, void *userData) {
    swapCallback = cb;
    swapParam = userData;
}

int DRM::setupFramebuffers() {

    drm_fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);

    printf("Becoming master of DRM device\n");

    // become master of drm device
    if(ioctl(drm_fd, DRM_IOCTL_SET_MASTER, 0) < 0) {
        printf("Error: Could not become device master\n");
        return 1;
    }

    drm_mode_card_res res={0};

    printf("Querying DRM resources\n");

    //Get resource counts
    if(ioctl(drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
        printf("Error: Could not get resources\n");
        return 1;
    }

    // TODO: Free malloc'd memory
    res.fb_id_ptr=(uint64_t)malloc(res.count_fbs * sizeof(uint32_t));
    res.crtc_id_ptr=(uint64_t)malloc(res.count_crtcs * sizeof(uint32_t));
    res.connector_id_ptr=(uint64_t)malloc(res.count_connectors * sizeof(uint32_t));
    res.encoder_id_ptr=(uint64_t)malloc(res.count_encoders * sizeof(uint32_t));
    //Get resource IDs
    if(ioctl(drm_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
        printf("Error: Could not get resources\n");
        return 1;
    }


    printf("Found %d connectors\n", res.count_connectors);

    int modeSelect = 6;

    // Iterate through connectors and create a framebuffer
    for(int i = 0; i < res.count_connectors; i++) {
        drm_mode_get_connector conn={0};

        conn.connector_id=((uint32_t*)res.connector_id_ptr)[i];
        printf("Getting details about connector %d\n", i);
        //get connector resource counts
        if(ioctl(drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
            printf("Error: Could not get connector\n");
            return 1;
        }

        drm_mode_modeinfo *connModes = (struct drm_mode_modeinfo *) malloc(conn.count_modes * sizeof(struct drm_mode_modeinfo));
        conn.modes_ptr = (uint64_t)connModes;
        conn.props_ptr = (uint64_t)malloc(conn.count_props * sizeof(uint32_t));
        conn.prop_values_ptr = (uint64_t)malloc(conn.count_props * sizeof(uint64_t));
        conn.encoders_ptr = (uint64_t)malloc(conn.count_props * sizeof(uint32_t));
        //get connector resource IDs
        if(ioctl(drm_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
            printf("Error: Could not get connector\n");
            return 1;
        }

        FrameBufferData fbData1;
        fbData1.conn_id = conn.connector_id;


        printf("Connector %d has %u encoders, %u modes, %u properties. Connected is %d\n", i, conn.count_encoders, conn.count_modes, conn.count_props, conn.connection);

        // connector is invalid or has no modes
        if (conn.count_encoders<1 || conn.count_modes<1 || !conn.encoder_id || !conn.connection) {
            printf("Unsuitable connector. Encoder id: %d\n", conn.encoder_id);
            continue;
        }

        // TODO: Just create a framebuffer for first valid connector for now
        drm_mode_create_dumb create_dumb={0};
        drm_mode_map_dumb map_dumb={0};
        drm_mode_fb_cmd cmd_dumb={0};

        // TODO: Just using predetermined mode for now
        memset(&create_dumb, 0, sizeof(drm_mode_create_dumb));
        create_dumb.width = connModes[modeSelect].hdisplay;
        create_dumb.height = connModes[modeSelect].vdisplay;
        create_dumb.bpp = 32;

        // TODO: Set up another framebuffer for double buffering
        printf("Creating dumb framebuffer\n");
        if(ioctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) < 0) {
            printf("Error: Could not create dumb framebuffer\n");
            return 1;
        }

        FrameBuffer fb1;
        
        fb1.width = create_dumb.width;
        fb1.height = create_dumb.height;
        fb1.size = create_dumb.size;
        fb1.pitch = create_dumb.pitch;
        fbData1.handle = create_dumb.handle;

        cmd_dumb.width=create_dumb.width;
        cmd_dumb.height=create_dumb.height;
        cmd_dumb.bpp=create_dumb.bpp;
        cmd_dumb.pitch=create_dumb.pitch;
        cmd_dumb.depth=24;
        cmd_dumb.handle=create_dumb.handle;
        if(ioctl(drm_fd,DRM_IOCTL_MODE_ADDFB,&cmd_dumb) < 0) {
            printf("Error: Could not register framebuffer\n");
            return 1;
        }

        printf("Registered framebuffer (%d): %dx%d, bpp=%d, pitch=%d\n", cmd_dumb.fb_id, cmd_dumb.width, cmd_dumb.height, cmd_dumb.bpp, cmd_dumb.pitch);
        fbData1.fb_id = cmd_dumb.fb_id;

        map_dumb.handle=create_dumb.handle;
        if(ioctl(drm_fd,DRM_IOCTL_MODE_MAP_DUMB,&map_dumb) < 0) {
            printf("Error: Could not map dumb framebuffer\n");
            return 1;
        }

        printf("Framebuffer prepared for mapping\n");

        // mmap the framebuffer
        fb1.buf = (Color *)mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, drm_fd, map_dumb.offset);
        fbData1.frameBuf = fb1;

        printf("Framebuffer mmapped at %p\n", fb1.buf);

        // Need to set up encoder
        // This is what will convert the framebuffer to what is needed for display
        drm_mode_get_encoder enc={0};

        enc.encoder_id=conn.encoder_id;
        printf("Querying active encoder ID %d\n", conn.encoder_id);
        //get already active encoder
        if(ioctl(drm_fd, DRM_IOCTL_MODE_GETENCODER, &enc) < 0) {
            printf("Error: Could not get encoder\n");
            return 1;
        }

        drm_mode_crtc crtc={0};
        memset(&old_crtc, 0, sizeof(drm_mode_crtc));

        old_crtc.crtc_id=enc.crtc_id;
        printf("Querying associated CRTC ID %d\n", enc.crtc_id);
         // get associated crtc
        if(ioctl(drm_fd, DRM_IOCTL_MODE_GETCRTC, &old_crtc) < 0) {
            printf("Error: Could not Get CRTC\n");
            return 1;
        }

        printf("Old CRTC: ID=%d, bufferID=%d, #connectors=%d, conn_ptr=%p, mode=%p, mode_valid=%d\n",old_crtc.crtc_id, old_crtc.fb_id, old_crtc.count_connectors, old_crtc.set_connectors_ptr, old_crtc.mode, old_crtc.mode_valid);

        printf("Attaching framebuffer to CRTC\n");
        // Connect CRTC to framebuffer
        crtc.mode = connModes[modeSelect];
        crtc.crtc_id = old_crtc.crtc_id;
        crtc.fb_id=cmd_dumb.fb_id;
        crtc.set_connectors_ptr=(uint64_t) &conn.connector_id;
        crtc.count_connectors=1;
        crtc.mode_valid=1;

        new_crtc = crtc;
        crtcMode= 1;
        if(ioctl(drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
            printf("Error: Could not set CRTC\n");
            return 1;
        }

        fbData1.crtc_id = crtc.crtc_id;
        fbData[0] = fbData1;
        printf("Framebuffer attached to CRTC %d\n", crtc.crtc_id);
        break;
    }

    return 0;
}

void DRM::swapBuffers() {
    drm_mode_crtc_page_flip flip;

    memset(&flip, 0, sizeof(flip));
    flip.fb_id = fbData[currentBuf].fb_id;
    flip.crtc_id = fbData[currentBuf].crtc_id;
    flip.user_data = (uint64_t) swapParam;
    // Requests vblank event after page flip
    flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
    flip.reserved = 0;

    if(ioctl(drm_fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip) < 0) {
        printf("Error: Could not trigger page flip\n");
    }

    // TODO: Toggle between 0 and 1
    //currentBuf ^= 1;
}

void DRM::onVBlankEvent(drm_event_vblank *vblank) {
    printf("On VBlank\n");
}

void DRM::onPageFlipCompleteEvent(drm_event_vblank *vblank) {
    void *userData = (void *) vblank->user_data;
    swapCallback(userData);
}

void DRM::onSequenceEvent(drm_event_crtc_sequence *seq) {
    printf("On Sequence\n");
}

int DRM::handleEvents() {
    pollfd fds[1] = {
        {.fd = drm_fd, .events = POLLIN},
    };

    if(poll(fds, 1, 1) < 0) {
        printf("Error on poll\n");
        return 1;;
    }


    // Nothing to read
    if(fds[0].revents & POLLIN == 0) {
        return 0;
    }


    char buffer[1024];
    int len, i;
    drm_event *e;
    drm_event_vblank *vblank;
    drm_event_crtc_sequence *seq;

    // Read from DRM device for events
    len = read(drm_fd, buffer, sizeof buffer);
    if (len == 0)
        return 0;
    if (len < (int)sizeof *e)
        return -1;

    i = 0;
    while (i < len) {
        e = (drm_event *)(buffer + i);
        switch (e->type) {
        case DRM_EVENT_VBLANK:
            vblank = (drm_event_vblank *) e;
            onVBlankEvent(vblank);
            break;
        case DRM_EVENT_FLIP_COMPLETE:
            vblank = (drm_event_vblank *) e;
            onPageFlipCompleteEvent(vblank);
            break;
        case DRM_EVENT_CRTC_SEQUENCE:
            seq = (drm_event_crtc_sequence *) e;
            onSequenceEvent(seq);
            break;
        default:
            break;
        }
        i += e->length;
    }
    return 0;
}

int DRM::setCRTC(int mode) {
    printf("Setting CRTC to mode %d\n", mode);
    drm_mode_crtc crtc = old_crtc;

    if(mode == 1) crtc = new_crtc;

    crtc.mode_valid = 1;
    crtc.count_connectors = 1;
    crtc.set_connectors_ptr = (uint64_t) &fbData[currentBuf].conn_id;
    printf("CRTC: ID=%d, bufferID=%d, #connectors=%d, mode=%p, mode_valid=%d\n",crtc.crtc_id, crtc.fb_id, crtc.count_connectors, crtc.mode, crtc.mode_valid);
    if(ioctl(drm_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
        printf("Error: Could not set CRTC\n");
        return 1;
    }

    return 0;
}

// Swap between old and new modes
// Useful for seeing printf output while it's running
void DRM::swapMode() {
    crtcMode ^= 1;
    setCRTC(crtcMode);
}

int DRM::cleanFramebuffers() {

    setCRTC(0);

    // TODO: Both fbs
    printf("Removing framebuffer %p (%d)\n", fbData[0].frameBuf.buf, fbData[0].fb_id);
    if(ioctl(drm_fd, DRM_IOCTL_MODE_RMFB, &fbData[0].fb_id) < 0) {
        printf("Error: Could not remove framebuffer\n");
    }

    drm_mode_destroy_dumb destroy = {0};
    destroy.handle = fbData[0].handle;
    printf("Destroying dumb framebuffer. Handle: %d\n", fbData[0].handle);
    if(ioctl(drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy) < 0) {
        printf("Error: Could not destroy dumb framebuffer\n");
    }
    
    printf("Unbecoming DRM master\n");
    // No longer need to be master of device
    if(ioctl(drm_fd, DRM_IOCTL_DROP_MASTER, 0)) {
        printf("Could not unbecome DRM master\n");
        return 1;
    }

    printf("Closing DRM device\n");
    close(drm_fd);
    return 0;
}