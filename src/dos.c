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

uint32_t timer_millis();

static bool use_dpmi_yield = false;

void sys_yield()
{
    if (use_dpmi_yield)
        __dpmi_yield();
}

bool sys_idle(int (*idle_callback)(), int idle_callback_period)
{
    sys_yield();
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

struct timer_slot_t {
    uint32_t id;
    pt_timer_callback callback;
    void* param;
    uint32_t interval;
    uint32_t count;
};

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
    struct timer_slot_t slots[256];
    uint32_t max_callback_id;
    int slot_head;
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
    // run the timer callbacks
    for (int i = 0; i < _timer.slot_head; i++) {
        _timer.slots[i].count++;
        if (_timer.slots[i].count == _timer.slots[i].interval) {
            uint32_t result = _timer.slots[i].callback(_timer.slots[i].interval, _timer.slots[i].param);
            if (result == 0) {
                // remove the callback
                _timer.slot_head--;
                if (i != _timer.slot_head) {
                    _timer.slots[i].callback = _timer.slots[_timer.slot_head].callback;
                    _timer.slots[i].count = _timer.slots[_timer.slot_head].count;
                    _timer.slots[i].id = _timer.slots[_timer.slot_head].id;
                    _timer.slots[i].interval = _timer.slots[_timer.slot_head].interval;
                    _timer.slots[i].param = _timer.slots[_timer.slot_head].param;
                }
                _timer.slots[_timer.slot_head].callback = NULL;
                _timer.slots[_timer.slot_head].count = 0;
                _timer.slots[_timer.slot_head].id = 0;
                _timer.slots[_timer.slot_head].interval = 0;
                _timer.slots[_timer.slot_head].param = NULL;
            } else {
                // set the new update interval
                _timer.slots[i].interval = result;
                _timer.slots[i].count = 0;
            }
        }
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
    log_print("dos:timer_init(): Replacing DOS timer interrupt...\n");
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
    log_print("dos:timer_init(): DOS timer hotpatched to run at %dHz\n", TIMER_HZ);
    errno = 0;
    // Check if we're running with an external DPMI server (e.g. Windows 98)
    __dpmi_yield();
    if (errno != ENOSYS) {
        use_dpmi_yield = true;
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
    // shortcut; we know the installed timer is 1000Hz
    if (_timer.installed)
        return _timer.ticks;
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
        sys_yield();
        end = timer_ticks();
    } while ((end - begin) < delay_ticks);
}

void timer_shutdown()
{
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

uint32_t timer_add_callback(uint32_t interval, pt_timer_callback callback, void* param)
{
    if (_timer.slot_head >= 256)
        return 0;

    _timer.max_callback_id++;
    _timer.slots[_timer.slot_head].id = _timer.max_callback_id;
    _timer.slots[_timer.slot_head].count = 0;
    _timer.slots[_timer.slot_head].interval = interval;
    _timer.slots[_timer.slot_head].callback = callback;
    _timer.slots[_timer.slot_head].param = param;
    _timer.slot_head++;
}

bool timer_remove_callback(uint32_t id)
{
    bool result = false;
    for (int i = 0; i < _timer.slot_head; i++) {
        if (_timer.slots[i].id == id) {
            _timer.slot_head--;
            if (_timer.slot_head == i) {
                // removed timer was in the last slot
                memset(&_timer.slots[i], 0, sizeof(struct timer_slot_t));
            } else if (_timer.slot_head > 0) {
                // move last slot into cleared slot
                memmove(&_timer.slots[i], &_timer.slots[_timer.slot_head], sizeof(struct timer_slot_t));
                // clear last slot
                memset(&_timer.slots[_timer.slot_head], 0, sizeof(struct timer_slot_t));
            }
            result = true;
            break;
        }
    }
    return result;
}

pt_drv_timer dos_timer = { &timer_init, &timer_shutdown, &timer_ticks, &timer_millis, &timer_sleep, &timer_add_callback,
    &timer_remove_callback };

void pcspeaker_stop();
void pcspeaker_init()
{
}

void pcspeaker_shutdown()
{
    pcspeaker_stop();
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

pt_drv_beep dos_beep = {
    &pcspeaker_init,
    &pcspeaker_shutdown,
    &pcspeaker_tone,
    &pcspeaker_stop,
};

// Serial port
// Info: https://www.lammertbies.nl/comm/info/serial-uart

static uint16_t serial_port_base = 0;

#define COM1_PORT 0x3f8
#define COM2_PORT 0x2f8
#define COM3_PORT 0x3e8
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
    // FIXME: Make this programmable
    serial_port_base = COM4_PORT;

    // COM4 - FIFO Control Register
    // - Clear Transmit FIFO
    // - Clear Receive FIFO
    // - Enable FIFO
    outportb(serial_port_base + SERIAL_IIR_FCR, 0x07);

    // COM4 - Line Control Register
    // Enable DLAB bit
    outportb(serial_port_base + SERIAL_LCR, 0x80);

    // COM4 - Divisor Latches
    // Crank this baby to 115200bps
    outportb(serial_port_base + SERIAL_RBR_THR, 0x01);
    outportb(serial_port_base + SERIAL_IER, 0x00);

    // COM4 - Line Control Register
    // Disable DLAB bit
    outportb(serial_port_base + SERIAL_LCR, 0x00);
}

bool serial_rx_ready()
{
    if (!serial_port_base)
        return false;

    // COM4 - Modem Status Register
    // Data set ready bit
    if ((inportb(serial_port_base + SERIAL_MSR) & 0x20) == 0)
        return false;

    // COM4 - Line Status Register
    // Data available bit
    if ((inportb(serial_port_base + SERIAL_LSR) & 0x01) == 0)
        return false;
    return true;
}

bool serial_tx_ready()
{
    if (!serial_port_base)
        return false;

    // COM4 - Line Status Register
    // TX Holding Register empty bit
    if ((inportb(serial_port_base + SERIAL_LSR) & 0x20) == 0)
        return false;

    // COM4 - Modem Status Register
    // Data set ready + Clear to send bits
    if ((inportb(serial_port_base + SERIAL_MSR) & 0x30) != 0x30)
        return false;

    return true;
}

byte serial_getc();

int serial_gets(byte* buffer, size_t length)
{
    size_t result = 0;
    if (!serial_port_base)
        return result;

    // COM4 - Modem Control Register
    // Data terminal ready + Request to send bits
    outportb(serial_port_base + SERIAL_MCR, 0x03);
    uint32_t millis = timer_millis();
    while ((result < length) && (timer_millis() < millis + 50)) {
        if (!serial_rx_ready()) {
            sys_yield();
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
    if (!serial_port_base)
        return 0xff;

    // COM4 - Receiver Buffer Register
    return inportb(serial_port_base + SERIAL_RBR_THR);
}

void serial_putc(byte data)
{
    if (!serial_port_base)
        return;

    // COM4 - Transmitter Holding Buffer
    outportb(serial_port_base + SERIAL_RBR_THR, data);
}

size_t serial_write(const void* buffer, size_t size)
{
    size_t result = 0;
    if (!serial_port_base)
        return result;

    // COM4 - Modem Control Register
    // Data terminal ready + Request to send bits
    outportb(serial_port_base + SERIAL_MCR, 0x03);

    while (result < size) {
        sys_yield();
        if (serial_tx_ready()) {
            serial_putc(((byte*)buffer)[result]);
            result++;
        }
    }

    // COM4 - Modem Control Register
    // Clear request to send bit
    outportb(serial_port_base + SERIAL_MCR, 0x01);
    return result;
}

int serial_printf(const char* format, ...)
{
    if (!serial_port_base)
        return 0;

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

pt_drv_serial dos_serial = {
    &serial_init,
    &serial_shutdown,
    &serial_rx_ready,
    &serial_tx_ready,
    &serial_getc,
    &serial_gets,
    &serial_putc,
    &serial_write,
    &serial_printf,
};

// Mouse handler
// Adapted from lovedos

int mouse_inited;
int mouse_x, mouse_y;
int mouse_last_x, mouse_last_y;
bool mouse_button_states[MOUSE_BUTTON_MAX];

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
                mouse_button_states[i] = t == 0;
            }
        }
    }
}

bool mouse_is_down(enum pt_mouse_button button)
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

pt_drv_mouse dos_mouse = {
    &mouse_init,
    &mouse_update,
    &mouse_shutdown,
    &mouse_get_x,
    &mouse_get_y,
    &mouse_is_down,
};

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
    // Disable all interrupts while installing the new keyboard interrupt
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
    _go32_dpmi_allocate_iret_wrapper(&keyboard_handler_new);
    _go32_dpmi_set_protected_mode_interrupt_vector(9, &keyboard_handler_new);
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
    _go32_dpmi_free_iret_wrapper(&keyboard_handler_new);
    enable();
}

pt_drv_keyboard dos_keyboard
    = { &keyboard_init, &keyboard_update, &keyboard_shutdown, &keyboard_set_key_repeat, &keyboard_is_down };

// Sound driver

#define OPL3_BASE_PORT 0x388

static bool sound_opl2_available = false;
static bool sound_opl3_available = false;

inline byte sound_opl3_status(uint16_t base)
{
    return inportb(base);
}

inline void sound_opl3_out_raw(uint16_t base, byte addr, byte data)
{
    outportb(base, addr);
    // have to wait for the card to switch registers
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);

    outportb(base + 1, data);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
    inportb(base);
}

void opl_write_reg(uint16_t addr, uint8_t data)
{
    if (sound_opl3_available) {
        sound_opl3_out_raw(OPL3_BASE_PORT + (addr & 0x100 ? 2 : 0), addr & 0xff, data);
    }
}

void _opl_lock()
{
    // Lock the memory page that contains all of the OPL code
    _go32_dpmi_lock_code(opl_write_reg, (uint32_t)((void*)_opl_lock - (void*)opl_write_reg));
    _go32_dpmi_lock_data((void*)&sound_opl2_available, sizeof(sound_opl2_available));
    _go32_dpmi_lock_data((void*)&sound_opl3_available, sizeof(sound_opl3_available));
}

// detection method nicked from https://moddingwiki.shikadi.net/wiki/OPL_chip#Detection_Methods
void opl_init()
{
    log_print("dos:opl_init: Detecting Yamaha OPL chip...\n");
    _opl_lock();

    // Reset timer 1 and timer 2
    sound_opl3_out_raw(OPL3_BASE_PORT, 0x4, 0x60);

    // Reset the IRQ
    sound_opl3_out_raw(OPL3_BASE_PORT, 0x4, 0x80);

    // Read status register
    byte status1 = sound_opl3_status(OPL3_BASE_PORT);

    // Set timer 1 to 0xff
    sound_opl3_out_raw(OPL3_BASE_PORT, 0x2, 0xff);

    // Unmask and start timer 1
    sound_opl3_out_raw(OPL3_BASE_PORT, 0x4, 0x21);

    // Wait for 80 microseconds
    timer_sleep(1);

    // Read status register
    byte status2 = sound_opl3_status(OPL3_BASE_PORT);

    // Reset timer 1 and timer 2
    sound_opl3_out_raw(OPL3_BASE_PORT, 0x4, 0x60);

    // Reset the IRQ
    sound_opl3_out_raw(OPL3_BASE_PORT, 0x4, 0x80);

    if (((status1 & 0xe0) != 0x00) || ((status2 & 0xe0) != 0xc0)) {
        log_print("dos:opl_init: Yamaha OPL not found: %02x %02x\n", status1, status2);
        return;
    }
    sound_opl2_available = true;
    if ((status2 & 0x06) != 0) {
        log_print("dos:opl_init: found Yamaha OPL2\n");
    } else {
        sound_opl3_available = true;
        log_print("dos:opl_init: found Yamaha OPL3\n");
    }
    // zero all the registers
    for (int i = 1; i < 0xf6; i++) {
        sound_opl3_out_raw(OPL3_BASE_PORT, i, 0);
        sound_opl3_out_raw(OPL3_BASE_PORT + 2, i, 0);
    }
}

void opl_shutdown()
{
    if (sound_opl2_available || sound_opl3_available) {
        for (int i = 1; i < 0xf6; i++) {
            sound_opl3_out_raw(OPL3_BASE_PORT, i, 0);
            sound_opl3_out_raw(OPL3_BASE_PORT + 2, i, 0);
        }
    }
    sound_opl2_available = false;
    sound_opl3_available = false;
}

pt_drv_opl dos_opl = { &opl_init, &opl_shutdown, &opl_write_reg };
