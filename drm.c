#include <stdio.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <drm.h>
//#include <drm/drm_mode.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <termios.h>

typedef struct Color {
    unsigned char b;
    unsigned char g;
    unsigned char r;
    unsigned char a;
} Color;

typedef struct DrawData {
	Color *frameBuffer;
	int width;
	int height;
	int fbSize;
	int pitch;

	int _fb_id;
	int _crtc_id;
	int _conn_id;
	int _handle;
	
	int mode;
	struct drm_mode_crtc old_crtc;
	struct drm_mode_crtc new_crtc;
} DrawData;

DrawData data;

void drawLoop();

int dri_fd = 0;
int virtualWidth = 0;
int tty;
struct vt_mode mode;

struct fb_var_screeninfo displayInfo(int fbfd) {
    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo;

    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        printf("Error reading fixed screeninfo");
        return vinfo;
    }
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        printf("Error reading variable screeninfo");
        return vinfo;
    }

    fprintf(stdout, "Fixed screen informations\n"
                    "-------------------------\n"
                    "Id string: %s\n"
                    "FB start memory: %p\n",
                    finfo.id, (void *)finfo.smem_start);

    fprintf(stdout, "Variable screen informations\n"
                    "----------------------------\n"
                    "xres: %d\n"
                    "yres: %d\n"
                    "xres_virtual: %d\n"
                    "yres_virtual: %d\n"
                    "bits_per_pixel: %d\n",
                    vinfo.xres, vinfo.yres, vinfo.xres_virtual,
                    vinfo.yres_virtual, vinfo.bits_per_pixel);
    return vinfo;
}

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(double msec)
{
    struct timespec ts;
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

/*
    Linearly interpolates from x1 to x2 as t goes from 0 to 1
*/
double lerp(double x1, double x2, double t) {
    return (x2 - x1) * t + x1;
}

Color lerpColor(Color p1, Color p2, double t) {
    Color p;
    p.r = floor(lerp(p1.r, p2.r, t));
    p.g = floor(lerp(p1.g, p2.g, t));
    p.b = floor(lerp(p1.b, p2.b, t));
    p.a = floor(lerp(p1.a, p2.a, t));
    return p;
}

Color colorRGBA(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    Color p = {.r = r, .g = g, .b = b, .a = a};
    return p;
}

Color getPixel(int x, int y) {
	Color c = {0, 0, 0, 0};
    if (x < 0 || y < 0 || x >= data.width || y >= data.height) {
        return c;
    }

	int index = y * virtualWidth + x;
	return data.frameBuffer[index];
}

void drawPixel(int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= data.width || y >= data.height) {
        return;
    }
    int index = y * data.width + x;
//	printf("X=%d Y=%d IDX=%d\n", x, y, index);
    data.frameBuffer[index] = color;
}

void drawRect(int x1, int y1, int x2, int y2, Color color) {
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            drawPixel(x, y, color);
        }
    }
}

void drawLine(int x1, int y1, int x2, int y2, Color color) {
    // Implement Bresenham's line drawing algorithm
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        drawPixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

void drawCircle(int xc, int yc, int radius, Color color) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    while (y >= x) {
        drawPixel(xc + x, yc + y, color);
        drawPixel(xc - x, yc + y, color);
        drawPixel(xc + x, yc - y, color);
        drawPixel(xc - x, yc - y, color);
        drawPixel(xc + y, yc + x, color);
        drawPixel(xc - y, yc + x, color);
        drawPixel(xc + y, yc - x, color);
        drawPixel(xc - y, yc - x, color);

        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }
}

void drawFilledCircle(int x0, int y0, int radius, Color color) {
    int x = radius;
    int y = 0;
    int radiusError = 1-x;

    while(x >= y) {
        drawLine(x0 + x, y0 + y, x0 - x, y0 + y, color);
        drawLine(x0 + y, y0 + x, x0 - y, y0 + x, color);
        drawLine(x0 - x, y0 - y, x0 + x, y0 - y, color);
        drawLine(x0 - y, y0 - x, x0 + y, y0 - x, color);
        y++;
        if (radiusError<0) {
            radiusError += 2 * y + 1;
        } else {
            x--;
            radiusError += 2 * (y - x + 1);
        }
    }
}

struct termios orig_termios, new_termios;
int setGraphicsMode() {
	int tty = open("/dev/tty", O_RDWR);
    if (tty < 0) {
        printf("Error opening terminal");
        return 1;
    }

    // Hide cursor
    //printf("\033[?25l");

    // Save current terminal attributes
    tcgetattr(0, &orig_termios);

    // Turn on raw mode
    new_termios = orig_termios;
	cfmakeraw(&new_termios);
	// Add back out processing
	new_termios.c_oflag |= (OPOST|ONLCR);
    tcsetattr(0, TCSANOW, &new_termios);

    // Save current terminal mode
    ioctl(tty, VT_GETMODE, &mode);

    // Set terminal to graphics mode
    /*struct vt_mode new_mode = { .mode = VT_PROCESS };
    ioctl(tty, VT_SETMODE, &new_mode);*/

	//ioctl(tty, KDSETMODE, KD_GRAPHICS);
	return 0;
}

void setTextMode() {
	//ioctl(tty, KDSETMODE, KD_TEXT);
    // Restore terminal mode
    tcsetattr(0, TCSANOW, &orig_termios);
    ioctl(tty, VT_SETMODE, &mode);
    // Show cursor
    printf("\033[?25h");
    close(tty);
}

void drawSinWave(double amplitude, double frequency, double offset,  Color color) {
    double x1, y1, x2, y2;
    for (int x = 0; x < data.width; x++) {
        x1 = x;
        y1 = amplitude * sin((2 * M_PI * frequency * x / data.width)+offset) + data.height/2;
        x2 = x+1;
        y2 = amplitude * sin((2 * M_PI * frequency * (x+1) / data.width)+offset) + data.height/2;
        drawLine(x1, y1, x2, y2, color);
    }
}

void drawWindow(int winX, int winY, int winW, int winH, Color winColor) {
	Color outline = colorRGBA(37, 112, 208, 255);
	Color titleColor = colorRGBA(239, 237, 224, 255);
	// Window
	drawRect(winX, winY, winX+winW, winY+winH, winColor);

	// Outline
	drawLine(winX, winY, winX+winW, winY, outline);
	drawLine(winX, winY, winX, winY+winH, outline);
	drawLine(winX+winW, winY, winX+winW, winY+winH, outline);
	drawLine(winX, winY+winH, winX+winW, winY+winH, outline);

	// Title
	int winTitleH = 16;
	drawRect(winX, winY-winTitleH, winX+winW, winY, outline);
}

int setupFramebuffer() {
	dri_fd = open("/dev/dri/card0", O_RDWR | O_NONBLOCK);

	printf("Becoming master of DRM device\n");

	// become master of drm device
	if(ioctl(dri_fd, DRM_IOCTL_SET_MASTER, 0) < 0) {
		printf("Error: Could not become device master\n");
		return 1;
	}

	struct drm_mode_card_res res={0};

	printf("Querying DRM resources\n");

	//Get resource counts
	if(ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		printf("Error: Could not get resources\n");
		return 1;
	}

	res.fb_id_ptr=(uint64_t)malloc(res.count_fbs * sizeof(uint32_t));
	res.crtc_id_ptr=(uint64_t)malloc(res.count_crtcs * sizeof(uint32_t));
	res.connector_id_ptr=(uint64_t)malloc(res.count_connectors * sizeof(uint32_t));
	res.encoder_id_ptr=(uint64_t)malloc(res.count_encoders * sizeof(uint32_t));
	//Get resource IDs
	if(ioctl(dri_fd, DRM_IOCTL_MODE_GETRESOURCES, &res) < 0) {
		printf("Error: Could not get resources\n");
		return 1;
	}


	printf("Found %d connectors\n", res.count_connectors);

	int modeSelect = 6;

	// Iterate through connectors and create a 
	for(int i = 0; i < res.count_connectors; i++) {

		struct drm_mode_get_connector conn={0};

		conn.connector_id=((uint32_t*)res.connector_id_ptr)[i];
		printf("Getting details about connector %d\n", i);
		//get connector resource counts
		if(ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
			printf("Error: Could not get connector\n");
			return 1;
		}

		struct drm_mode_modeinfo *connModes = (struct drm_mode_modeinfo *) malloc(conn.count_modes * sizeof(struct drm_mode_modeinfo));
		conn.modes_ptr = (uint64_t)connModes;
		conn.props_ptr = (uint64_t)malloc(conn.count_props * sizeof(uint32_t));
		conn.prop_values_ptr = (uint64_t)malloc(conn.count_props * sizeof(uint64_t));
		conn.encoders_ptr = (uint64_t)malloc(conn.count_props * sizeof(uint32_t));
		//get connector resource IDs
		if(ioctl(dri_fd, DRM_IOCTL_MODE_GETCONNECTOR, &conn) < 0) {
			printf("Error: Could not get connector\n");
			return 1;
		}

		data._conn_id = conn.connector_id;


		printf("Connector %d has %u encoders, %u modes, %u properties. Connected is %d\n", i, conn.count_encoders, conn.count_modes, conn.count_props, conn.connection);

		// connector is invalid or has no modes
		if (conn.count_encoders<1 || conn.count_modes<1 || !conn.encoder_id || !conn.connection) {
			printf("Unsuitable connector. Encoder id: %d\n", conn.encoder_id);
			continue;
		}

		// TODO: Just create a framebuffer for first valid connector for now
		struct drm_mode_create_dumb create_dumb={0};
		struct drm_mode_map_dumb map_dumb={0};
		struct drm_mode_fb_cmd cmd_dumb={0};

		// TODO: Just using predetermined mode for now
		memset(&create_dumb, 0, sizeof(struct drm_mode_create_dumb));
		create_dumb.width = connModes[modeSelect].hdisplay;
		create_dumb.height = connModes[modeSelect].vdisplay;
		create_dumb.bpp = 32;

		printf("Creating dumb framebuffer\n");
		if(ioctl(dri_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_dumb) < 0) {
			printf("Error: Could not create dumb framebuffer\n");
			return 1;
		}

		data.width = create_dumb.width;
		data.height = create_dumb.height;
		data.fbSize = create_dumb.size;
		data.pitch = create_dumb.pitch;
		data._handle = create_dumb.handle;

		cmd_dumb.width=create_dumb.width;
		cmd_dumb.height=create_dumb.height;
		cmd_dumb.bpp=create_dumb.bpp;
		cmd_dumb.pitch=create_dumb.pitch;
		cmd_dumb.depth=24;
		cmd_dumb.handle=create_dumb.handle;
		if(ioctl(dri_fd,DRM_IOCTL_MODE_ADDFB,&cmd_dumb) < 0) {
			printf("Error: Could not register framebuffer\n");
			return 1;
		}

		printf("Registered framebuffer (%d): %dx%d, bpp=%d, pitch=%d\n", cmd_dumb.fb_id, cmd_dumb.width, cmd_dumb.height, cmd_dumb.bpp, cmd_dumb.pitch);
		data._fb_id = cmd_dumb.fb_id;

		map_dumb.handle=create_dumb.handle;
		if(ioctl(dri_fd,DRM_IOCTL_MODE_MAP_DUMB,&map_dumb) < 0) {
			printf("Error: Could not map dumb framebuffer\n");
			return 1;
		}

		printf("Framebuffer prepared for mapping\n");

		// mmap the framebuffer
		data.frameBuffer = (Color *)mmap(0, create_dumb.size, PROT_READ | PROT_WRITE, MAP_SHARED, dri_fd, map_dumb.offset);

		printf("Framebuffer mmapped at %p\n", data.frameBuffer);

		// Need to set up encoder
		// This is what will convert the framebuffer to what is needed for display
		struct drm_mode_get_encoder enc={0};

		enc.encoder_id=conn.encoder_id;
		printf("Querying active encoder ID %d\n", conn.encoder_id);
		//get already active encoder
		if(ioctl(dri_fd, DRM_IOCTL_MODE_GETENCODER, &enc) < 0) {
			printf("Error: Could not get encoder\n");
			return 1;
		}

		struct drm_mode_crtc crtc={0};
		memset(&data.old_crtc, 0, sizeof(struct drm_mode_crtc));

		data.old_crtc.crtc_id=enc.crtc_id;
		printf("Querying associated CRTC ID %d\n", enc.crtc_id);
		 // get associated crtc
		if(ioctl(dri_fd, DRM_IOCTL_MODE_GETCRTC, &data.old_crtc) < 0) {
			printf("Error: Could not Get CRTC\n");
			return 1;
		}

		printf("Old CRTC: ID=%d, bufferID=%d, #connectors=%d, conn_ptr=%p, mode=%p, mode_valid=%d\n",data.old_crtc.crtc_id, data.old_crtc.fb_id, data.old_crtc.count_connectors, data.old_crtc.set_connectors_ptr, data.old_crtc.mode, data.old_crtc.mode_valid);

		printf("Attaching framebuffer to CRTC\n");
		// Connect CRTC to framebuffer
		crtc.mode = connModes[modeSelect];
		crtc.crtc_id = data.old_crtc.crtc_id;
		crtc.fb_id=cmd_dumb.fb_id;
		crtc.set_connectors_ptr=(uint64_t) &conn.connector_id;
		crtc.count_connectors=1;
		crtc.mode_valid=1;

		data.new_crtc = crtc;
		data.mode = 1;
		if(ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
			printf("Error: Could not set CRTC\n");
			return 1;
		}

		data._crtc_id = crtc.crtc_id;
		printf("Framebuffer attached to CRTC %d\n", crtc.crtc_id);
		break;
	}

	return 0;
}

void pageFlip() {
	struct drm_mode_crtc_page_flip flip;

	memset(&flip, 0, sizeof(flip));
	flip.fb_id = data._fb_id;
	flip.crtc_id = data._crtc_id;
	flip.user_data = (uint64_t) &data;
	// Requests vblank event after page flip
	flip.flags = DRM_MODE_PAGE_FLIP_EVENT;
	flip.reserved = 0;

	if(ioctl(dri_fd, DRM_IOCTL_MODE_PAGE_FLIP, &flip) < 0) {
		printf("Error: Could not trigger page flip\n");
	}
}

void onVBlankEvent(struct drm_event_vblank *vblank) {
	printf("On VBlank\n");
}

void onPageFlipCompleteEvent(struct drm_event_vblank *vblank) {
	// if we need it
	//void *userData = (void *) vblank.user_data;
	printf("On page flip\n");
	drawLoop();
}

void onSequenceEvent(struct drm_event_crtc_sequence *seq) {
	printf("On Sequence\n");
}

int handleDRMEvents() {
	char buffer[1024];
	int len, i;
	struct drm_event *e;
	struct drm_event_vblank *vblank;
	struct drm_event_crtc_sequence *seq;
	void *user_data;

	// Read from DRM device for events
	len = read(dri_fd, buffer, sizeof buffer);
	if (len == 0)
		return 0;
	if (len < (int)sizeof *e)
		return -1;

	i = 0;
	while (i < len) {
		e = (struct drm_event *)(buffer + i);
		switch (e->type) {
		case DRM_EVENT_VBLANK:
			vblank = (struct drm_event_vblank *) e;
			onVBlankEvent(vblank);
			break;
		case DRM_EVENT_FLIP_COMPLETE:
			vblank = (struct drm_event_vblank *) e;
			onPageFlipCompleteEvent(vblank);
			break;
		case DRM_EVENT_CRTC_SEQUENCE:
			seq = (struct drm_event_crtc_sequence *) e;
			onSequenceEvent(seq);
			break;
		default:
			break;
		}
		i += e->length;
	}
	return 0;
}

int setCRTC(int mode) {
	printf("Setting CRTC to mode %d\n", mode);
	struct drm_mode_crtc crtc = data.old_crtc;

	if(mode == 1) crtc = data.new_crtc;

	crtc.mode_valid = 1;
	crtc.count_connectors = 1;
	crtc.set_connectors_ptr = (uint64_t) &data._conn_id;
	printf("CRTC: ID=%d, bufferID=%d, #connectors=%d, mode=%p, mode_valid=%d\n",crtc.crtc_id, crtc.fb_id, crtc.count_connectors, crtc.mode, crtc.mode_valid);
	if(ioctl(dri_fd, DRM_IOCTL_MODE_SETCRTC, &crtc) < 0) {
		printf("Error: Could not set CRTC\n");
		return 1;
	}

	return 0;
}

int cleanFramebuffer() {

	setCRTC(0);

	printf("Removing framebuffer %p (%d)\n", data.frameBuffer, data._fb_id);
	if(ioctl(dri_fd, DRM_IOCTL_MODE_RMFB, &data._fb_id) < 0) {
		printf("Error: Could not remove framebuffer\n");
	}

	struct drm_mode_destroy_dumb destroy = {0};
	destroy.handle = data._handle;
	printf("Destroying dumb framebuffer. Handle: %d\n", data._handle);
	if(ioctl(dri_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy) < 0) {
		printf("Error: Could not destroy dumb framebuffer\n");
	}
	
	printf("Unbecoming DRM master\n");
	// No longer need to be master of device
	if(ioctl(dri_fd, DRM_IOCTL_DROP_MASTER, 0)) {
		printf("Could not unbecome DRM master\n");
		return 1;
	}

	printf("Closing DRM device\n");
	close(dri_fd);
}

void drawLoop() {
	int winW = 640;
	int winH = 480;
	int winX = data.width/2-winW/2;
	int winY = data.height/2-winH/2;
	double r = 32;

	double x = winX+winW/2;
	double y = winY+48;

	printf("Clearing background\n");
	memset(data.frameBuffer, 128, data.fbSize);

	printf("Drawing window\n");
	Color winColor = colorRGBA(255, 255, 255, 255);
	drawWindow(winX, winY, winW, winH, winColor);

	printf("Draw loop\n");
	int g = rand() % 255;
	drawFilledCircle(floor(x), floor(y), r, colorRGBA(255, g, 20, 255));

	pageFlip();
}

int main() {
    
	if(setupFramebuffer() > 0) {
		printf("Error: Could not setup framebuffer\n");
		return 1;
	}

	if(setGraphicsMode() > 0) {
		printf("Could not set graphics mode");
		return 1;
	}

	drawLoop();

	struct timeval timeout;

	fd_set fds;
	while(1) {
		// Select can modify this, so reset
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;
		FD_ZERO(&fds);
		FD_SET(STDIN_FILENO, &fds);
		FD_SET(dri_fd, &fds);
		int num_fds = 2;
		if(select(num_fds, &fds, NULL, NULL, &timeout) < 0) {
			printf("Error on select\n");
			continue;
		}

		if(FD_ISSET(STDIN_FILENO, &fds)) {
			char c = getchar();
			if(c == 'q') {
				printf("Quitting\n");
				break;
			} else if(c == 'h') {
				printf("Toggling display mode\n");
				// toggles between 1 and 0
				data.mode ^= 1;
				if(setCRTC(data.mode) < 0) {
					printf("Error setting CRTC to mode %d", data.mode);
				}
			}
		}
		if(FD_ISSET(dri_fd, &fds)) {
			printf("Reading from DRM\n");
			if(handleDRMEvents() < 0) {
				printf("Error handling DRM events\n");
			}
		}
	}

	// Clear
//	sleep(5);
	drawRect(0, 0, data.width, data.height, colorRGBA(0, 0, 0, 255));

	cleanFramebuffer();
	setTextMode();
    return 0;
}

