#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <bios.h>
#include <dos.h>
#include <dpmi.h>
#include <errno.h>
#include <fcntl.h>
#include <go32.h>
#include <sys/nearptr.h>
#include <termios.h>
#include <unistd.h>

#include "dos.h"
#include "image.h"
#include "log.h"
#include "rect.h"

typedef unsigned char byte;
typedef unsigned short word;

typedef struct pt_image_vga pt_image_vga;
struct pt_image_vga {
    byte *data;
    uint16_t width;
    uint16_t height;
    int16_t origin_x;
    int16_t origin_y;
    uint16_t pitch; 
};


uint8_t video_map_colour(uint8_t r, uint8_t b, uint8_t g) {
    
}

pt_image_vga *video_convert_image(pt_image *image) {
    pt_image_vga *result = (pt_image_vga *)calloc(1, sizeof(pt_image_vga));
    result->width = image->width;
    result->height = image->height;
    result->origin_x = image->origin_x;
    result->origin_y = image->origin_y;
    result->pitch = image->pitch;
    result->data = (byte *)calloc(result->pitch * result->height, sizeof(byte));
    // inform the DOS driver what the colour palette is from the image
    // - driver updates free palette slots as required
    // - if we're out of colour slots, the driver should return the nearest match
    // generate a lookup table for mapping old palette to new palette
    // - for every colour in the old palette, find the index in the new palette
    //
    return result;
}



// VGA blitter

inline byte *vga_ptr() {
    return (byte *)0xA0000 + __djgpp_conventional_base;
}

static byte *framebuffer = NULL;

void video_shutdown();

void video_init() {
    // Memory protection is for chumps
    if (__djgpp_nearptr_enable() == 0) {
        log_print("Couldn't access the first 640K of memory. Boourns.");
        exit(-1);
    }
    // Mode 13h - raster, 256 colours, 320x200
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    framebuffer = (byte *)calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(byte));
    atexit(video_shutdown);
}

void video_clear() {
    if (!framebuffer)
        return;
    memset(framebuffer, 0, SCREEN_WIDTH*SCREEN_HEIGHT);
}

void video_blit_image(pt_image *image, int16_t x, int16_t y) {
    if (!framebuffer) {
        log_print("framebuffer missing ya dingus!\n");

        return;
    }
    if (!image) {
        log_print("WARNING: Tried to blit nothing ya dingus\n");
        return;
    }

    struct rect *ir = create_rect_dims(image->width, image->height);
    
    struct rect *crop = create_rect_dims(SCREEN_WIDTH, SCREEN_HEIGHT);
    x -= image->origin_x;
    y -= image->origin_y;
    if (!rect_blit_clip(&x, &y, ir, crop)) {
        log_print("Rectangle off screen\n");
        destroy_rect(ir);
        destroy_rect(crop);
        return;
    }
   
    //log_print("Blitting %s to %d,%d %dx%d\n", image->path, x, y, image->width, image->height);
    int16_t width = rect_width(ir);
    for (int yi = ir->top; yi < ir->bottom; ) {
        memcpy(
            (framebuffer + (y << 8) + (y << 6) + x),
            (image->data + (yi * image->pitch) + ir->left),
            width
        );
        yi++;
        y++;
    }
    destroy_rect(ir);
    destroy_rect(crop);
}

bool video_is_vblank() {
    // sleep until the start of the next vertical blanking interval
    // CRT mode and status - CGA/EGA/VGA input status 1 register - in vertical retrace
    return inportb(0x3da) & 8; 
}

void video_flip() {
    // copy the framebuffer
    if (!framebuffer)
        return;
    memcpy(vga_ptr(), framebuffer, SCREEN_WIDTH*SCREEN_HEIGHT);
}

void video_shutdown() {
    // Mode 3h - text, 16 colours, 80x25
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x03;
    int86(0x10, &regs, &regs);
    // Fine, maybe we should stop being unsafe
    __djgpp_nearptr_disable();
    free(framebuffer);
    framebuffer = NULL;
}

bool sys_idle(int (*idle_callback)(), int idle_callback_period) {
    __dpmi_yield();
    if (idle_callback && (timer_millis() % idle_callback_period == 0))
        return idle_callback() > 0;
    return 1;
}

// Timer interupt replacement
// Adapted from PCTIMER 1.4 by Chih-Hao Tsai

#define TIMER_TERM_FAIL 0
#define TIMER_TERM_CHAIN 1
#define TIMER_TERM_OUTPORTB 2

// PIT clock frequency
#define TIMER_PIT_CLOCK 1193180L
// timer frequency we want
#define TIMER_HZ 1000
// default timer values in the BIOS
#define TIMER_DEFAULT_FREQ 18.2067597
#define TIMER_DEFAULT_TICK_PER_MS 0.0182068
#define TIMER_DEFAULT_MS_PER_TICK 54.9246551


struct timer_data_t {
    int16_t counter;
    int16_t reset;
    uint32_t ticks;
    int flag;
    uint8_t term;
    bool installed; 
    float freq;
    _go32_dpmi_seginfo handler_real_old;
    _go32_dpmi_seginfo handler_real_new;
    _go32_dpmi_seginfo handler_prot_old;
    _go32_dpmi_seginfo handler_prot_new;
    _go32_dpmi_registers r;
    _go32_dpmi_registers r1;
};
static struct timer_data_t _timer = { 0 };
static float _timer_tick_per_ms = TIMER_DEFAULT_TICK_PER_MS;
static float _timer_ms_per_tick = TIMER_DEFAULT_MS_PER_TICK;
static float _timer_freq = TIMER_DEFAULT_FREQ;

void _int_8h_prot() {
    // Replacement for timer interrupt 8h in protected mode 
    // disable other interrupts  
    disable();
    _timer.ticks++;
    _timer.counter++;

    if (_timer.counter == _timer.reset) {
        _timer.flag = TIMER_TERM_CHAIN;
        _timer.counter = 0;
    } else {
        _timer.flag = TIMER_TERM_OUTPORTB;
        outportb(0x20, 0x20);
    }
    enable();
}

void _int_8h_prot_lock() {
    // Lock the memory page that contains _int_8h_prot()
    _go32_dpmi_lock_code(_int_8h_prot, (uint32_t)(_int_8h_prot_lock - _int_8h_prot));
}

void _int_8h_real() {
    // Real mode interrupt handler
    
    // Disable all other interrupts while processing
    disable();
    
    if (_timer.flag == TIMER_TERM_FAIL) {
        _timer.ticks++;
        _timer.counter++;
    }
    if ((_timer.counter == _timer.reset) || (_timer.flag == TIMER_TERM_CHAIN)) {
        _timer.flag = TIMER_TERM_FAIL;
        _timer.counter = 0;
        memset(&_timer.r, 0, sizeof(_timer.r));
        _timer.r.x.cs = _timer.handler_real_old.rm_segment;
        _timer.r.x.ip = _timer.handler_real_old.rm_offset;
_timer.r.x.ss = _timer.r.x.sp = 0;
        _go32_dpmi_simulate_fcall_iret(&_timer.r);
    } else {
        _timer.flag = TIMER_TERM_FAIL;
        // PIC1 command register - end of interrupt
        outportb(0x20, 0x20);
    }
    enable();
}

void _int_8h_real_lock() {
    // Lock the memory page that contains _int_8h_prot()
    _go32_dpmi_lock_code(_int_8h_real, (uint32_t)(_int_8h_real_lock - _int_8h_real));
}

void timer_shutdown();

void timer_init() {
    if (!_timer.installed) {
        // Disable all interrupts while installing the new timer
        disable();

        // Lock code memory used by the two new handlers
        _int_8h_real_lock();
        _int_8h_prot_lock();

        // Lock data memory used by timer state
        _go32_dpmi_lock_data(&_timer, sizeof(_timer));

        // Chain the new protected mode handler to 8h
        _go32_dpmi_get_protected_mode_interrupt_vector(8, &_timer.handler_prot_old);
        _timer.handler_prot_new.pm_offset = (uint32_t)_int_8h_prot;
        _timer.handler_prot_new.pm_selector = (uint32_t)_go32_my_cs();
        _go32_dpmi_chain_protected_mode_interrupt_vector(8, &_timer.handler_prot_new);

        // Inject a shim for the existing real mode handler
        _go32_dpmi_get_real_mode_interrupt_vector(8, &_timer.handler_real_old);
        _timer.handler_real_new.pm_offset = (uint32_t)_int_8h_real;
        _go32_dpmi_allocate_real_mode_callback_iret(&_timer.handler_real_new, &_timer.r1);
        _go32_dpmi_set_real_mode_interrupt_vector(8, &_timer.handler_real_new);

        // https://wiki.osdev.org/Programmable_Interval_Timer
        // PIT command register - channel 1, high byte, mode 2 (rate generator) 
        outportb(0x43, 0x36);
        // PIT0 data register - load in our new frequency
        outportb(0x40, (TIMER_PIT_CLOCK / TIMER_HZ) & 0xff); 
        outportb(0x40, (TIMER_PIT_CLOCK / TIMER_HZ) >> 8); 
        _timer.ticks = 0;
        _timer.counter = 0;
        _timer.reset = TIMER_HZ / TIMER_DEFAULT_FREQ;

        // set up measurement constants
        _timer_freq = TIMER_HZ;
        _timer_tick_per_ms = _timer_freq / 1000;
        _timer_ms_per_tick = 1000 / _timer_freq;
        _timer.installed = true;
        enable();
        
        atexit(timer_shutdown);
    }
}

uint32_t timer_ticks() {
    if (_timer.installed) {
        // use the ticks tracked from our handler
        return _timer.ticks;
    }
    // ask the BIOS for the number of ticks
    return biostime(0, 0);
}

uint32_t timer_millis() {
    return timer_ticks()*_timer_ms_per_tick;
}

void timer_sleep(uint32_t delay_millis) {
    // force a wait of at least one tick
    uint32_t delay_ticks = delay_millis * _timer_tick_per_ms + 1;
    uint32_t begin = 0;
    uint32_t end = 0;
    begin = timer_ticks();

    // Blocking loop until enough ticks have passed
    do {
        // Yield/halt CPU until next interrupt
        __dpmi_yield();
        end = timer_ticks();
    } while ((end - begin) < delay_ticks);
}


void timer_shutdown() {
    if (_timer.installed) {
        disable();
        // PIT control register - 
        outportb(0x43, 0x36);
        // PIT0 data port - zero it out
        outportb(0x40, 0x00);
        outportb(0x40, 0x00);

        // Restore protected mode 8h handler
        _go32_dpmi_set_protected_mode_interrupt_vector(8, &_timer.handler_prot_old);
        // Restore real mode 8h handler and free the shim
        _go32_dpmi_set_real_mode_interrupt_vector(8, &_timer.handler_real_old);
        _go32_dpmi_free_real_mode_callback(&_timer.handler_real_new);
        enable();
        
        // set the BIOS tick counter to (RTC time) * default PIT frequency  
        _go32_dpmi_registers r;
        memset(&r, 0, sizeof(r));
        // INT 1Ah - real time clock
        // AH=02 - read RTC time as binary-coded decimal
        r.h.ah = 0x02;
        _go32_dpmi_simulate_int(0x1a, &r);

        uint32_t tick = TIMER_DEFAULT_FREQ * (
            ((float) ((r.h.ch & 0xf) + ((r.h.ch >> 4) & 0xf) * 10) * 3600) +
            ((float) ((r.h.cl & 0xf) + ((r.h.cl >> 4) & 0xf) * 10) * 60) +
            ((float) ((r.h.dh & 0xf) + ((r.h.dh >> 4) & 0xf) * 10))
        );
        biostime(1, tick);

        // reset everything back to defaults
        _timer.installed = false;
        _timer.freq = TIMER_DEFAULT_FREQ;
        _timer.reset = 1;
        _timer_tick_per_ms = TIMER_DEFAULT_TICK_PER_MS;
        _timer_ms_per_tick = TIMER_DEFAULT_MS_PER_TICK;
    }
}


void pcspeaker_tone(float freq) {
    uint32_t units = TIMER_PIT_CLOCK / freq;
    // PIT mode/command register
    outportb(0x43, 0xb6);
    // Load tone frequency into PIT channel 2 data port
    outportb(0x42, units & 0xff);
    outportb(0x42, units >> 8);
    // Enable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) | 3);
}

void pcspeaker_stop() {
    // Disable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) & 0xfc);
}

// Serial port

void serial_test() {
    // Used for debug console.
    // Rely on DJGPP's POSIX compatibility layer for doing all the work.
    int port = open("COM4", O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (port < 0) {
        log_print("Couldn't open serial (%d): %s", errno, strerror(errno));
        return;
    }
    // adjust the termios settings for our COM port
    struct termios tty;
    if (!tcgetattr(port, &tty)) {
        log_print("Couldn't fetch termios settings for port (%d): %s", errno, strerror(errno));
        close(port);
        return;
    }
    // Disable parity bit
    tty.c_cflag &= ~PARENB;
    // 1 stop bit
    tty.c_cflag &= ~CSTOPB;
    // 8 bits per byte
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    // 38400 baud, fastest speed known to science
    tty.c_cflag |= B38400;
    // Begone, worthless modem control lines
    tty.c_cflag |= CLOCAL;

    if (!tcsetattr(port, TCSANOW, &tty)) {
        log_print("Couldn't set termios settings for port (%d): %s", errno, strerror(errno));
        close(port);
        return;
    }

    FILE *ttyfp = fdopen(port, "r+");
    if (!ttyfp) {
        log_print("Couldn't get file pointer for port (%d): %s", errno, strerror(errno));
        close(port);
        return;
    }


    for (int i = 0; i < 20; i++) {
        log_print("Debug line %d\n", i);
        fflush(ttyfp);
        sleep(1);
    }
    fclose(ttyfp);

}

// Mouse handler
// Adapted from lovedos

int mouse_inited;
int mouse_x, mouse_y;
int mouse_lastX, mouse_lastY;
int mouse_buttonStates[MOUSE_BUTTON_MAX];

void mouse_init() {
    union REGS regs = {};
    // INT 33h AH=0 Mouse - Reset driver and read status
    int86(0x33, &regs, &regs);
    mouse_inited = regs.x.ax ? 1 : 0;
}

void mouse_update() {
    if (!mouse_inited) {
        return;
    }

    /* Update mouse position */
    union REGS regs = {};
    regs.x.ax = 3;
    int86(0x33, &regs, &regs);
    mouse_x = regs.x.cx >> 1;
    mouse_y = regs.x.dx;

    /* Do moved event if mouse moved */
    /*if (mouse_x != mouse_lastX || mouse_y != mouse_lastY) {
        event_t e;
        e.type = EVENT_MOUSE_MOVED;
        e.mouse.x = mouse_x;
        e.mouse.y = mouse_y;
        e.mouse.dx = mouse_x - mouse_lastX;
        e.mouse.dy = mouse_y - mouse_lastY;
        event_push(&e);
    }*/

    /* Update last position */
    mouse_lastX = mouse_x;
    mouse_lastY = mouse_y;

    /* Update button states and push pressed/released events */
    int i, t;
    for (i = 0; i < MOUSE_BUTTON_MAX; i++) {
        for (t = 0; t < 2; t++) {
            memset(&regs, 0, sizeof(regs));
            regs.x.ax = 5 + t;
            regs.x.bx = i;
            int86(0x33, &regs, &regs);
            if (regs.x.bx) {
                /* Push event */
                /*event_t e;
                e.type = t == 0 ? EVENT_MOUSE_PRESSED : EVENT_MOUSE_RELEASED;
                e.mouse.button = i;
                e.mouse.x = mouse_x;
                e.mouse.y = mouse_y;
                event_push(&e);*/
                /* Set state */
                mouse_buttonStates[i] = t == 0 ? 1 : 0;
            }
        }
    }
}

int mouse_is_down(int button) { return mouse_buttonStates[button]; }

int mouse_get_x() { return mouse_x; }

int mouse_get_y() { return mouse_y; }
