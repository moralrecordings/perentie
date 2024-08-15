#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bios.h>
#include <dos.h>
#include <dpmi.h>
#include <errno.h>
#include <fcntl.h>
#include <go32.h>
#include <signal.h>
#include <sys/nearptr.h>
#include <termios.h>
#include <unistd.h>

#include "dos.h"
#include "event.h"
#include "image.h"
#include "log.h"
#include "rect.h"

// VGA blitter

// clang-format off
static byte vga_palette[256*3] = {
    0, 0, 0, 
    0, 0, 42,
    0, 42, 0,
    0, 42, 42,
    42, 0, 0,
    42, 0, 42,
    42, 21, 0,
    42, 42, 42,
    21, 21, 21,
    21, 21, 63,
    21, 63, 21,
    21, 63, 63,
    63, 21, 21,
    63, 21, 63,
    63, 63, 21,
    63, 63, 63,
};
// clang-format on
static int vga_palette_top = 16;

inline byte* vga_ptr()
{
    return (byte*)0xA0000 + __djgpp_conventional_base;
}

static byte* framebuffer = NULL;

void video_shutdown();

void video_init()
{
    // Memory protection is for chumps
    if (__djgpp_nearptr_enable() == 0) {
        log_print("Couldn't access the first 640K of memory. Boourns.\n");
        exit(-1);
    }
    // Mode 13h - raster, 256 colours, 320x200
    union REGS regs;
    regs.h.ah = 0x00;
    regs.h.al = 0x13;
    int86(0x10, &regs, &regs);

    framebuffer = (byte*)calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(byte));
    atexit(video_shutdown);
}

void video_clear()
{
    if (!framebuffer)
        return;
    memset(framebuffer, 0, SCREEN_WIDTH * SCREEN_HEIGHT);
}

void video_blit_image(pt_image* image, int16_t x, int16_t y)
{
    if (!framebuffer) {
        log_print("framebuffer missing ya dingus!\n");

        return;
    }
    if (!image) {
        log_print("WARNING: Tried to blit nothing ya dingus\n");
        return;
    }

    if (!image->hw_image) {
        image->hw_image = (void*)video_convert_image(image);
    }

    pt_image_vga* hw_image = (pt_image_vga*)image->hw_image;

    struct rect* ir = create_rect_dims(hw_image->width, hw_image->height);

    struct rect* crop = create_rect_dims(SCREEN_WIDTH, SCREEN_HEIGHT);
    x -= image->origin_x;
    y -= image->origin_y;
    // log_print("Blitting %s to (%d,%d) %dx%d ->", image->path, x, y, image->width, image->height);
    //  Constrain x and y to be an absolute start offset in screen space.
    //  Crop the image rectangle, based on its location in screen space.
    if (!rect_blit_clip(&x, &y, ir, crop)) {
        // log_print("Rectangle off screen\n");
        destroy_rect(ir);
        destroy_rect(crop);
        return;
    }
    // log_print("image coords (%d,%d) %dx%d\n", ir->left, ir->top, rect_width(ir), rect_height(ir));

    // The source data is then drawn from the image rectangle.
    int16_t x_start = x;
    for (int yi = ir->top; yi < ir->bottom; yi++) {
        for (int xi = ir->left; xi < ir->right; xi += sizeof(uint8_t)) {
            size_t hw_offset = (yi * hw_image->pitch) + xi;
            uint8_t mask = *(uint8_t*)(hw_image->mask + hw_offset);
            uint8_t* ptr = (uint8_t*)(framebuffer + (y << 8) + (y << 6) + x);
            *ptr = (*ptr & ~mask) | (*(uint8_t*)(hw_image->bitmap + hw_offset) & mask);
            x += sizeof(uint8_t);
        }
        y++;
        x = x_start;
    }
    destroy_rect(ir);
    destroy_rect(crop);
}

bool video_is_vblank()
{
    // sleep until the start of the next vertical blanking interval
    // CRT mode and status - CGA/EGA/VGA input status 1 register - in vertical retrace
    return inportb(0x3da) & 8;
}

void video_flip()
{
    // copy the framebuffer
    if (!framebuffer)
        return;
    memcpy(vga_ptr(), framebuffer, SCREEN_WIDTH * SCREEN_HEIGHT);
}

void video_load_palette_colour(int idx)
{
    disable();
    outportb(0x3c6, 0xff);
    outportb(0x3c8, (uint8_t)idx);
    outportb(0x3c9, vga_palette[3 * idx]);
    outportb(0x3c9, vga_palette[3 * idx + 1]);
    outportb(0x3c9, vga_palette[3 * idx + 2]);
    enable();
}

// clang-format off
uint8_t vga_remap[] = {
    0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02,
    0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x04,
    0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x06,
    0x06, 0x06, 0x06, 0x07, 0x07, 0x07, 0x07, 0x08,
    0x08, 0x08, 0x08, 0x09, 0x09, 0x09, 0x09, 0x0a,
    0x0a, 0x0a, 0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0c,
    0x0c, 0x0c, 0x0c, 0x0d, 0x0d, 0x0d, 0x0d, 0x0e,
    0x0e, 0x0e, 0x0e, 0x0f, 0x0f, 0x0f, 0x0f, 0x10,
    0x10, 0x10, 0x10, 0x11, 0x11, 0x11, 0x11, 0x12,
    0x12, 0x12, 0x12, 0x13, 0x13, 0x13, 0x13, 0x14,
    0x14, 0x14, 0x14, 0x15, 0x15, 0x15, 0x15, 0x15,
    0x16, 0x16, 0x16, 0x16, 0x17, 0x17, 0x17, 0x17,
    0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19,
    0x1a, 0x1a, 0x1a, 0x1a, 0x1b, 0x1b, 0x1b, 0x1b,
    0x1c, 0x1c, 0x1c, 0x1c, 0x1d, 0x1d, 0x1d, 0x1d,
    0x1e, 0x1e, 0x1e, 0x1e, 0x1f, 0x1f, 0x1f, 0x1f,
    0x20, 0x20, 0x20, 0x20, 0x21, 0x21, 0x21, 0x21,
    0x22, 0x22, 0x22, 0x22, 0x23, 0x23, 0x23, 0x23,
    0x24, 0x24, 0x24, 0x24, 0x25, 0x25, 0x25, 0x25,
    0x26, 0x26, 0x26, 0x26, 0x27, 0x27, 0x27, 0x27,
    0x28, 0x28, 0x28, 0x28, 0x29, 0x29, 0x29, 0x29,
    0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2b, 0x2b, 0x2b,
    0x2b, 0x2c, 0x2c, 0x2c, 0x2c, 0x2d, 0x2d, 0x2d,
    0x2d, 0x2e, 0x2e, 0x2e, 0x2e, 0x2f, 0x2f, 0x2f,
    0x2f, 0x30, 0x30, 0x30, 0x30, 0x31, 0x31, 0x31,
    0x31, 0x32, 0x32, 0x32, 0x32, 0x33, 0x33, 0x33,
    0x33, 0x34, 0x34, 0x34, 0x34, 0x35, 0x35, 0x35,
    0x35, 0x36, 0x36, 0x36, 0x36, 0x37, 0x37, 0x37,
    0x37, 0x38, 0x38, 0x38, 0x38, 0x39, 0x39, 0x39,
    0x39, 0x3a, 0x3a, 0x3a, 0x3a, 0x3b, 0x3b, 0x3b,
    0x3b, 0x3c, 0x3c, 0x3c, 0x3c, 0x3d, 0x3d, 0x3d,
    0x3d, 0x3e, 0x3e, 0x3e, 0x3e, 0x3f, 0x3f, 0x3f
};
// clang-format on

uint8_t video_map_colour(uint8_t r, uint8_t g, uint8_t b)
{
    byte r_v = vga_remap[r];
    byte g_v = vga_remap[g];
    byte b_v = vga_remap[b];
    for (int i = 0; i < vga_palette_top; i++) {
        if (vga_palette[3 * i] == r_v && vga_palette[3 * i + 1] == g_v && vga_palette[3 * i + 2] == b_v)
            return i;
    }
    // add a new colour
    if (vga_palette_top < 256) {
        int idx = vga_palette_top;
        vga_palette_top++;
        vga_palette[3 * idx] = r_v;
        vga_palette[3 * idx + 1] = g_v;
        vga_palette[3 * idx + 2] = b_v;
        video_load_palette_colour(idx);
        log_print("video_map_colour: vga_palette[%d] = %d, %d, %d -> %d, %d, %d\n", idx, r, g, b, r_v, g_v, b_v);
        return idx;
    }
    // Out of palette slots; need to macguyver the nearest colour.
    // Formula borrowed from ScummVM's palette code.
    uint8_t best_color = 0;
    uint32_t min = 0xffffffff;
    for (int i = 0; i < 256; ++i) {
        int rmean = (vga_palette[3 * i + 0] + r_v) / 2;
        int dr = vga_palette[3 * i + 0] - r_v;
        int dg = vga_palette[3 * i + 1] - g_v;
        int db = vga_palette[3 * i + 2] - b_v;

        uint32_t dist_squared = (((512 + rmean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - rmean) * db * db) >> 8);
        if (dist_squared < min) {
            best_color = i;
            min = dist_squared;
        }
    }
    return best_color;
}

pt_image_vga* video_convert_image(pt_image* image)
{
    pt_image_vga* result = (pt_image_vga*)calloc(1, sizeof(pt_image_vga));
    result->width = image->width;
    result->height = image->height;
    result->pitch = image->pitch;
    result->bitmap = (byte*)calloc(result->pitch * result->height, sizeof(byte));
    result->mask = (byte*)calloc(result->pitch * result->height, sizeof(byte));

    byte palette_map[256];
    for (int i = 0; i < 256; i++) {
        palette_map[i] = video_map_colour(image->palette[3 * i], image->palette[3 * i + 1], image->palette[3 * i + 2]);
    }

    for (int y = 0; y < result->height; y++) {
        for (int x = 0; x < result->width; x++) {
            result->bitmap[y * result->pitch + x] = palette_map[image->data[y * result->pitch + x]];
            result->mask[y * result->pitch + x] = image->data[y * result->pitch + x] == image->colourkey ? 0x00 : 0xff;
        }
    }

    return result;
}

void video_destroy_hw_image(void* hw_image)
{
    pt_image_vga* image = (pt_image_vga*)hw_image;
    if (!image)
        return;
    if (image->bitmap) {
        free(image->bitmap);
        image->bitmap = NULL;
    }
    if (image->mask) {
        free(image->mask);
        image->mask = NULL;
    }
    free(image);
}

void video_shutdown()
{
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

bool sys_idle(int (*idle_callback)(), int idle_callback_period)
{
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

void _int_8h_prot()
{
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

void _int_8h_prot_lock()
{
    // Lock the memory page that contains _int_8h_prot()
    _go32_dpmi_lock_code(_int_8h_prot, (uint32_t)(_int_8h_prot_lock - _int_8h_prot));
}

void _int_8h_real()
{
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

void _int_8h_real_lock()
{
    // Lock the memory page that contains _int_8h_real()
    _go32_dpmi_lock_code(_int_8h_real, (uint32_t)(_int_8h_real_lock - _int_8h_real));
}

void timer_shutdown();

void timer_init()
{
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

uint32_t timer_ticks()
{
    if (_timer.installed) {
        // use the ticks tracked from our handler
        return _timer.ticks;
    }
    // ask the BIOS for the number of ticks
    return biostime(0, 0);
}

uint32_t timer_millis()
{
    return timer_ticks() * _timer_ms_per_tick;
}

void timer_sleep(uint32_t delay_millis)
{
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

void timer_shutdown()
{
    if (_timer.installed) {
        disable();
        // Silence the PC speaker
        pcspeaker_stop();

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

        uint32_t tick = TIMER_DEFAULT_FREQ
            * (((float)((r.h.ch & 0xf) + ((r.h.ch >> 4) & 0xf) * 10) * 3600)
                + ((float)((r.h.cl & 0xf) + ((r.h.cl >> 4) & 0xf) * 10) * 60)
                + ((float)((r.h.dh & 0xf) + ((r.h.dh >> 4) & 0xf) * 10)));
        biostime(1, tick);

        // reset everything back to defaults
        _timer.installed = false;
        _timer.freq = TIMER_DEFAULT_FREQ;
        _timer.reset = 1;
        _timer_tick_per_ms = TIMER_DEFAULT_TICK_PER_MS;
        _timer_ms_per_tick = TIMER_DEFAULT_MS_PER_TICK;
    }
}

void pcspeaker_tone(float freq)
{
    uint32_t units = TIMER_PIT_CLOCK / freq;
    // PIT mode/command register
    outportb(0x43, 0xb6);
    // Load tone frequency into PIT channel 2 data port
    outportb(0x42, units & 0xff);
    outportb(0x42, units >> 8);
    // Enable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) | 3);
}

void pcspeaker_stop()
{
    // Disable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) & 0xfc);
}

// Serial port
// Info: https://www.lammertbies.nl/comm/info/serial-uart

#define COM4_PORT 0x2e8
#define SERIAL_RBR_THR 0
#define SERIAL_IER 1
#define SERIAL_IIR_FCR 2
#define SERIAL_LCR 3
#define SERIAL_MCR 4
#define SERIAL_LSR 5
#define SERIAL_MSR 6

void serial_init()
{
    // COM4 - FIFO Control Register
    // - Clear Transmit FIFO
    // - Clear Receive FIFO
    // - Enable FIFO
    outportb(COM4_PORT + SERIAL_IIR_FCR, 0x07);

    // COM4 - Line Control Register
    // Enable DLAB bit
    outportb(COM4_PORT + SERIAL_LCR, 0x80);

    // COM4 - Divisor Latches
    // Crank this baby to 115200bps
    outportb(COM4_PORT + SERIAL_RBR_THR, 0x01);
    outportb(COM4_PORT + SERIAL_IER, 0x00);

    // COM4 - Line Control Register
    // Disable DLAB bit
    outportb(COM4_PORT + SERIAL_LCR, 0x00);
}

bool serial_rx_ready()
{
    // COM4 - Modem Status Register
    // Data set ready bit
    if ((inportb(COM4_PORT + SERIAL_MSR) & 0x20) == 0)
        return false;

    // COM4 - Line Status Register
    // Data available bit
    if ((inportb(COM4_PORT + SERIAL_LSR) & 0x01) == 0)
        return false;
    return true;
}

bool serial_tx_ready()
{
    // COM4 - Line Status Register
    // TX Holding Register empty bit
    if ((inportb(COM4_PORT + SERIAL_LSR) & 0x20) == 0)
        return false;

    // COM4 - Modem Status Register
    // Data set ready + Clear to send bits
    if ((inportb(COM4_PORT + SERIAL_MSR) & 0x30) != 0x30)
        return false;

    return true;
}

int serial_gets(byte* buffer, size_t length)
{
    size_t result = 0;
    // COM4 - Modem Control Register
    // Data terminal ready + Request to send bits
    outportb(COM4_PORT + SERIAL_MCR, 0x03);
    uint32_t millis = timer_millis();
    while ((result < length) && (timer_millis() < millis + 10)) {
        if (!serial_rx_ready()) {
            __dpmi_yield();
            continue;
        }
        buffer[result] = serial_getc();
        result++;
        millis = timer_millis();
    }
    return result;
}

byte serial_getc()
{
    // COM4 - Receiver Buffer Register
    return inportb(COM4_PORT + SERIAL_RBR_THR);
}

void serial_putc(byte data)
{
    // COM4 - Transmitter Holding Buffer
    outportb(COM4_PORT + SERIAL_RBR_THR, data);
}

size_t serial_write(const void* buffer, size_t size)
{
    // COM4 - Modem Control Register
    // Data terminal ready + Request to send bits
    outportb(COM4_PORT + SERIAL_MCR, 0x03);

    size_t result = 0;
    while (result < size) {
        __dpmi_yield();
        if (serial_tx_ready()) {
            serial_putc(((byte*)buffer)[result]);
            result++;
        }
    }

    // COM4 - Modem Control Register
    // Clear request to send bit
    outportb(COM4_PORT + SERIAL_MCR, 0x01);
    return result;
}

int serial_printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);

    char buffer[1024];
    int result = vsnprintf(buffer, sizeof(buffer), format, args);

    result = serial_write(buffer, result);
    va_end(args);
    return result;
}

void serial_shutdown()
{
}

// Mouse handler
// Adapted from lovedos

int mouse_inited;
int mouse_x, mouse_y;
int mouse_last_x, mouse_last_y;
int mouse_button_states[MOUSE_BUTTON_MAX];

void mouse_init()
{
    union REGS regs = {};
    // INT 33h AH=0 Mouse - Reset driver and read status
    int86(0x33, &regs, &regs);
    mouse_inited = regs.x.ax ? 1 : 0;
}

void mouse_update()
{
    if (!mouse_inited) {
        return;
    }

    // INT 33h AH=3 - Return position and button status
    union REGS regs = {};
    regs.x.ax = 3;
    int86(0x33, &regs, &regs);
    mouse_x = regs.x.cx >> 1;
    mouse_y = regs.x.dx;

    // If the mouse moved, push a movement event
    if (mouse_x != mouse_last_x || mouse_y != mouse_last_y) {
        pt_event* ev = event_push(EVENT_MOUSE_MOVE);
        ev->mouse.x = mouse_x;
        ev->mouse.y = mouse_y;
        ev->mouse.dx = mouse_x - mouse_last_x;
        ev->mouse.dy = mouse_y - mouse_last_y;
    }

    // Keep track of last seen mouse position
    mouse_last_x = mouse_x;
    mouse_last_y = mouse_y;

    int i, t;
    for (i = 0; i < MOUSE_BUTTON_MAX; i++) {
        for (t = 0; t < 2; t++) {

            // INT 33h AH=5 - Return button press data
            // INT 33h AH=6 - Return button release data
            memset(&regs, 0, sizeof(regs));
            regs.x.ax = 5 + t;
            regs.x.bx = i;
            int86(0x33, &regs, &regs);

            if (regs.x.bx) {
                // Push event to queue
                pt_event* ev = event_push(t == 0 ? EVENT_MOUSE_DOWN : EVENT_MOUSE_UP);
                ev->mouse.button = 1 << i;
                ev->mouse.x = mouse_x;
                ev->mouse.y = mouse_y;
                // Save the button states
                mouse_button_states[i] = t == 0 ? 1 : 0;
            }
        }
    }
}

int mouse_is_down(int button)
{
    return mouse_button_states[button];
}

int mouse_get_x()
{
    return mouse_x;
}

int mouse_get_y()
{
    return mouse_y;
}

void mouse_shutdown()
{
    // noop
}

// Keyboard handler
// Adapted from lovedos

// clang-format off
static const char *DOS_SCANCODES[] = {
    [0] = "?",        [1] = "escape",     [2] = "1",           [3] = "2",
    [4] = "3",        [5] = "4",          [6] = "5",           [7] = "6",
    [8] = "7",        [9] = "8",          [10] = "9",          [11] = "0",
    [12] = "-",       [13] = "=",         [14] = "backspace",  [15] = "tab",
    [16] = "q",       [17] = "w",         [18] = "e",          [19] = "r",
    [20] = "t",       [21] = "y",         [22] = "u",          [23] = "i",
    [24] = "o",       [25] = "p",         [26] = "[",          [27] = "]",
    [28] = "return",  [29] = "lctrl",     [30] = "a",          [31] = "s",
    [32] = "d",       [33] = "f",         [34] = "g",          [35] = "h",
    [36] = "j",       [37] = "k",         [38] = "l",          [39] = ";",
    [40] = "\"",      [41] = "`",         [42] = "lshift",     [43] = "\\",
    [44] = "z",       [45] = "x",         [46] = "c",          [47] = "v",
    [48] = "b",       [49] = "n",         [50] = "m",          [51] = ",",
    [52] = ".",       [53] = "/",         [54] = "rshift",     [55] = "*",
    [56] = "lalt",    [57] = "space",     [58] = "capslock",   [59] = "f1",
    [60] = "f2",      [61] = "f3",        [62] = "f4",         [63] = "f5",
    [64] = "f6",      [65] = "f7",        [66] = "f8",         [67] = "f9",
    [68] = "f10",     [69] = "numlock",   [70] = "scrolllock", [71] = "home",
    [72] = "up",      [73] = "pageup",    [74] = "-",          [75] = "left",
    [76] = "5",       [77] = "right",     [78] = "+",          [79] = "end",
    [80] = "down",    [81] = "pagedown",  [82] = "insert",     [83] = "delete",
    [84] = "?",       [85] = "?",         [86] = "?",          [87] = "?",
    [88] = "f12",     [89] = "?",         [90] = "?",          [91] = "?",
    [92] = "?",       [93] = "?",         [94] = "?",          [95] = "?",
    [96] = "enter",   [97] = "lctrl",     [98] = "/",          [99] = "f12",
    [100] = "rctrl",  [101] = "pause",    [102] = "home",      [103] = "up",
    [104] = "pageup", [105] = "left",     [106] = "right",     [107] = "end",
    [108] = "down",   [109] = "pagedown", [110] = "insert",    [111] = "?",
    [112] = "?",      [113] = "?",        [114] = "?",         [115] = "?",
    [116] = "?",      [117] = "?",        [118] = "?",         [119] = "pause",
    [120] = "?",      [121] = "?",        [122] = "?",         [123] = "?",
    [124] = "?",      [125] = "?",        [126] = "?",         [127] = "?",
    [128] = NULL,
};
// clang-format on

#define KEY_MAX 128
#define KEY_BUFFER_SIZE 32
#define KEY_BUFFER_MASK (KEY_BUFFER_SIZE - 1)

static bool keyboard_allow_key_repeat = false;
static char keyboard_key_states[KEY_MAX];
static uint8_t keyboard_flags = 0;

enum { KEYPRESSED, KEYRELEASED };
typedef struct {
    unsigned char type, code, isrepeat;
} keyevent;

// Circular buffer for keyboard events from the interrupt handler.
struct {
    keyevent data[KEY_BUFFER_SIZE];
    uint8_t readi, writei;
} keyevent_buffer;

_go32_dpmi_seginfo keyboard_handler_old, keyboard_handler_new;

void _int_9h_prot()
{
    disable();
    // If the buffer is full, drop the event
    if (keyevent_buffer.readi == ((keyevent_buffer.writei + 1) % KEY_BUFFER_SIZE)) {
        return;
    }

    // INT 9H - IRQ1 - Keyboard Data Ready
    static uint8_t code;
    // KB controller data output buffer
    code = inportb(0x60);

    if (code != 224) {
        keyevent* e;
        if (code & 0x80) {
            // Key up event
            code &= ~0x80;
            keyboard_key_states[code] = 0;
            e = &keyevent_buffer.data[keyevent_buffer.writei & KEY_BUFFER_MASK];
            e->type = KEYRELEASED;
            e->code = code;
            e->isrepeat = 0;
            keyevent_buffer.writei++;

        } else {
            // Key down event
            int isrepeat = keyboard_key_states[code];
            if (!isrepeat || keyboard_allow_key_repeat) {
                keyboard_key_states[code] = 1;
                e = &keyevent_buffer.data[keyevent_buffer.writei & KEY_BUFFER_MASK];
                e->type = KEYPRESSED;
                e->code = code;
                e->isrepeat = isrepeat;
                keyevent_buffer.writei++;
            }
            if (code == 0x3a) { // caps lock
                keyboard_flags ^= KEY_FLAG_CAPS;
            } else if (code == 0x45) { // num lock
                keyboard_flags ^= KEY_FLAG_NUM;
            } else if (code == 0x46) { // scroll lock
                keyboard_flags ^= KEY_FLAG_SCRL;
            }
        }
    }
    // PIC1 command register - end of interrupt
    outportb(0x20, 0x20);
    enable();
}

void _int_9h_prot_lock()
{
    // Lock the memory page that contains _int_9h_prot()
    _go32_dpmi_lock_code(_int_9h_prot, (uint32_t)(_int_9h_prot_lock - _int_9h_prot));
}

void keyboard_init()
{
    // Disable all interrupts while installing the new timer
    disable();

    // Zero out the key event buffer
    memset(&keyevent_buffer, 0, sizeof(keyevent_buffer));

    // Lock code memory used by the new handler
    _int_9h_prot_lock();

    _go32_dpmi_lock_data((char*)&keyboard_key_states, KEY_MAX);
    _go32_dpmi_lock_data((char*)&keyboard_flags, sizeof(uint8_t));
    _go32_dpmi_lock_data((char*)&keyevent_buffer, sizeof(keyevent_buffer));

    // Replace the protected mode keyboard handler
    _go32_dpmi_get_protected_mode_interrupt_vector(9, &keyboard_handler_old);
    memset(&keyboard_handler_new, 0, sizeof(_go32_dpmi_seginfo));
    keyboard_handler_new.pm_offset = (uint32_t)_int_9h_prot;
    keyboard_handler_new.pm_selector = (uint32_t)_go32_my_cs();
    _go32_dpmi_chain_protected_mode_interrupt_vector(9, &keyboard_handler_new);
    enable();

    // Reset the keyboard LEDs
    // Keyboard Controller - set/reset mode indicators
    outportb(0x60, 0xed);
    outportb(0x60, 0x00);
}

void keyboard_set_key_repeat(bool allow)
{
    keyboard_allow_key_repeat = allow;
}

bool keyboard_is_down(const char* key)
{
    for (int i = 0; DOS_SCANCODES[i]; i++) {
        if (!strcmp(DOS_SCANCODES[i], key)) {
            return keyboard_key_states[i] == 1;
        }
    }
    return false;
}

void keyboard_update()
{
    // Move keyevents from the circular buffer to the event pipeline
    while (keyevent_buffer.readi != keyevent_buffer.writei) {
        keyevent* ke = &keyevent_buffer.data[keyevent_buffer.readi++ & KEY_BUFFER_MASK];
        pt_event* ev = event_push(ke->type == KEYPRESSED ? EVENT_KEY_DOWN : EVENT_KEY_UP);
        ev->keyboard.key = DOS_SCANCODES[ke->code];
        ev->keyboard.isrepeat = ke->isrepeat;
        ev->keyboard.flags = keyboard_flags;
        if (keyboard_key_states[0x1d] || keyboard_key_states[0x64]) { // lctrl | rctrl
            ev->keyboard.flags |= KEY_FLAG_CTRL;
        }
        if (keyboard_key_states[0x38]) { // lalt
            ev->keyboard.flags |= KEY_FLAG_ALT;
        }
        if (keyboard_key_states[0x2a] || keyboard_key_states[0x36]) { // rshift
            ev->keyboard.flags |= KEY_FLAG_SHIFT;
        }
    }
}

void keyboard_shutdown()
{
    disable();
    // Restore protected mode 9h handler
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &keyboard_handler_old);
    enable();
}
