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

extern char etext;

uint32_t timer_millis();

static bool use_dpmi_yield = false;

void dos_yield()
{
    if (use_dpmi_yield)
        __dpmi_yield();
}

bool sys_idle(int (*idle_callback)(), int idle_callback_period)
{
    dos_yield();
    if (idle_callback && (timer_millis() % idle_callback_period == 0))
        return idle_callback() > 0;
    return 1;
}

// Timer interrupt replacement
// Heavily reworked from PCTIMER 1.4 by Chih-Hao Tsai
// Info: https://www.xtof.info/Timing-on-PC-familly-under-DOS.html

#define TIMER_TERM_FAIL 0
#define TIMER_TERM_CHAIN 1
#define TIMER_TERM_OUTPORTB 2

// PIT clock frequency
#define TIMER_PIT_CLOCK 1193180L
// timer frequency we want
#define TIMER_HZ 1000
#define TIMER_HIRES_HZ 16000
#define TIMER_HIRES_DIV 16
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
    uint32_t hiresticks;
    uint32_t oldticks;
    int flag;
    uint8_t term;
    bool installed;
    bool hires;
    bool real_mode_compat;
    float freq;
    _go32_dpmi_seginfo handler_prot_old;
    _go32_dpmi_seginfo handler_prot_new;
    struct timer_slot_t slots[256];
    uint32_t max_callback_id;
    int slot_head;
};
static struct timer_data_t _timer = { 0 };
static float _timer_tick_per_ms = TIMER_DEFAULT_TICK_PER_MS;
static float _timer_ms_per_tick = TIMER_DEFAULT_MS_PER_TICK;
static float _timer_freq = TIMER_DEFAULT_FREQ;

#define PCSPEAKER_OFF 0
#define PCSPEAKER_TONE 1
#define PCSPEAKER_SAMPLE_8K 2
#define PCSPEAKER_SAMPLE_16K 3
struct pcspeaker_data_t {
    uint8_t mode;
    byte* sample_data;
    size_t sample_len;
    size_t offset;
};
static struct pcspeaker_data_t _pcspeaker = { 0 };
static uint8_t pcspeaker_lut[256] = { 0 };
void pcspeaker_sample_update();

inline uint32_t* bios_ticks_ptr()
{
    return (uint32_t*)(0x46c + __djgpp_conventional_base);
}

static bool _int_8h_prot()
{
    // Replacement for timer interrupt 8h in protected mode

    // You'll notice this doesn't have enable() and disable().
    // There is a line of fine print in the DJGPP docs saying that you
    // shouldn't use them with _go32_dpmi_chain_protected_mode_interrupt_vector();
    // on real hardware this results in the game hanging on startup,
    // or better yet crashing with random registers being trashed.

    if (_timer.hires) {
        _timer.hiresticks++;
        pcspeaker_sample_update();
    }

    if (!_timer.hires || (_timer.hiresticks % TIMER_HIRES_DIV == 0)) {
        _timer.ticks++;
        _timer.counter++;

        // Emulate the old, crap timer.
        // _timer.reset is the number of ticks to approximate TIMER_DEFAULT_FREQ.
        // If we've hit that, enable the real mode handler.
        if (_timer.counter == _timer.reset) {
            _timer.flag = TIMER_TERM_CHAIN;
            _timer.counter = 0;
            _timer.oldticks++;
        } else {
            _timer.flag = TIMER_TERM_OUTPORTB;
            // PIC1 command register - end of interrupt
            outportb(0x20, 0x20);
        }
        // run the timer callbacks
        for (int i = 0; i < _timer.slot_head; i++) {
            _timer.slots[i].count++;
            if (_timer.slots[i].count == _timer.slots[i].interval) {
                uint32_t result
                    = _timer.slots[i].callback(_timer.slots[i].param, _timer.slots[i].id, _timer.slots[i].interval);
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
    } else {
        _timer.flag = TIMER_TERM_OUTPORTB;
        // PIC1 command register - end of interrupt
        outportb(0x20, 0x20);
    }

    // The real_mode_compat flag rigs the handler to always call the old code.
    // This is required for calls to the Microsoft Mouse DOS driver, which pulls crap like this:
    // 000073E1: cmp     cl,es:[di]      # 0000:[046c]
    // 000073E4: je      73E1h
    // This is an example of code which busy-loop polls 0000:046C for a change (aka the magic
    // spot where the BIOS keeps the number of timer ticks) to establish at least one timer interrupt
    // has happened, and (sometimes?!) IRQ0 only fires once then gives up.
    return (_timer.flag == TIMER_TERM_CHAIN) || _timer.real_mode_compat;
}

// Timer interrupt chain wrapper
// Adapted from djlsr205/src/libc/go32/gopint.c + gormcb.c by DJ Delorie/C. Sandmann
#define FILL 0x00

// This is basically the same wrapper code from
// _go32_dpmi_chain_protected_mode_interrupt_vector,
// but modded so that it only chains to the old handler
// if the new handler returns 1.
static unsigned char int8_prot_wrapper[] = {
    /* 00 */ 0x1e, /*     push ds              	*/
    /* 01 */ 0x06, /*     push es			*/
    /* 02 */ 0x0f, 0xa0, /*     push fs              	*/
    /* 04 */ 0x0f, 0xa8, /*     push gs              	*/
    /* 06 */ 0x60, /*     pusha                	*/
    /* 07 */ 0x66, 0xb8, /*     mov ax,			*/
    /* 09 */ FILL, FILL, /*         _our_selector	*/
    /* 0B */ 0x8e, 0xd8, /*     mov ds, ax           	*/
    /* 0D */ 0xff, 0x05, /*     incl			*/
    /* 0F */ FILL, FILL, FILL, FILL, /*	    _call_count		*/
    /* 13 */ 0x83, 0x3d, /*     cmpl			*/
    /* 15 */ FILL, FILL, FILL, FILL, /*         _in_this_handler	*/
    /* 19 */ 0x00, /*         $0			*/
    /* 1A */ 0x75, /*     jne			*/
    /* 1B */ 0x56, /*         bypass (-1c)		*/
    /* 1C */ 0xc6, 0x05, /*     movb			*/
    /* 1E */ FILL, FILL, FILL, FILL, /*         _in_this_handler 	*/
    /* 22 */ 0x01, /*         $1			*/
    /* 23 */ 0x8e, 0xc0, /*     mov es, ax           	*/
    /* 25 */ 0x8e, 0xe0, /*     mov fs, ax           	*/
    /* 27 */ 0x8e, 0xe8, /*     mov gs, ax           	*/
    /* 29 */ 0xbb, /*     mov ebx,			*/
    /* 2A */ FILL, FILL, FILL, FILL, /*         _local_stack		*/
    /* 2E */ 0xfc, /*     cld                  	*/
    /* 2F */ 0x89, 0xe1, /*     mov ecx, esp         	*/
    /* 31 */ 0x8c, 0xd2, /*     mov dx, ss           	*/
    /* 33 */ 0x8e, 0xd0, /*     mov ss, ax           	*/
    /* 35 */ 0x89, 0xdc, /*     mov esp, ebx         	*/
    /* 37 */ 0x52, /*     push edx             	*/
    /* 38 */ 0x51, /*     push ecx             	*/
    /* 39 */ 0xe8, /*     call			*/
    /* 3A */ FILL, FILL, FILL, FILL, /*         _rmih		*/
    /* 3E */ 0x09, 0xc0, /*     or eax, eax              */
    /* 40 */ 0x74, /*      jz      */
    /* 41 */ 0x23, /*      skip  */

    // New handler returned true, chain the old handler

    /* 42 */ 0x58, /*     pop eax                 	*/
    /* 43 */ 0x5b, /*     pop ebx                 	*/
    /* 44 */ 0x8e, 0xd3, /*     mov ss, bx               */
    /* 46 */ 0x89, 0xc4, /*     mov esp, eax             */
    /* 48 */ 0xc6, 0x05, /*     movb			*/
    /* 4A */ FILL, FILL, FILL, FILL, /*         _in_this_handler	*/
    /* 4E */ 0x00, /*         $0			*/
    /* 4F */ 0x61, /*     popa		*/
    /* 50 */ 0x90, /*     nop			*/
    /* 51 */ 0x0f, 0xa9, /*     pop gs                   */
    /* 53 */ 0x0f, 0xa1, /*     pop fs                   */
    /* 55 */ 0x07, /*     pop es                   */
    /* 56 */ 0x1f, /*     pop ds                   */
    /* 57 */ 0x2e, 0xff, 0x2d, /*     jmp     cs: 		*/
    /* 5A */ FILL, FILL, FILL, FILL, /*     [see below] 		*/
    /* 5E */ 0xcf, /*     iret                     */
    /* 5F */ FILL, FILL, FILL, FILL, /*     dd old_address 		*/
    /* 63 */ FILL, FILL, /*     dw old_segment 		*/

    // New handler returned false, clean everything up and iret

    /* 65 */ 0x58, /*     skip: pop eax                 	*/
    /* 66 */ 0x5b, /*     pop ebx                 	*/
    /* 67 */ 0x8e, 0xd3, /*     mov ss, bx               */
    /* 69 */ 0x89, 0xc4, /*     mov esp, eax             */
    /* 6B */ 0xc6, 0x05, /*     movb			*/
    /* 6D */ FILL, FILL, FILL, FILL, /*         _in_this_handler	*/
    /* 71 */ 0x00, /*         $0			*/
    /* 72 */ 0x61, /* bypass:  popa		*/
    /* 73 */ 0x90, /*     nop			*/
    /* 74 */ 0x0f, 0xa9, /*     pop gs                   */
    /* 76 */ 0x0f, 0xa1, /*     pop fs                   */
    /* 78 */ 0x07, /*     pop es                   */
    /* 79 */ 0x1f, /*     pop ds                   */
    /* 7A */ 0xcf, /*     iret                     */
};

static uint8_t* int8_prot_stack;

extern unsigned short __djgpp_ds_alias;

void _setup_int8_wrapper()
{
    int8_prot_stack = (uint8_t*)calloc(_go32_interrupt_stack_size, sizeof(uint8_t));
    _go32_dpmi_lock_data(int8_prot_stack, _go32_interrupt_stack_size);
    _go32_dpmi_lock_data(int8_prot_wrapper, sizeof(int8_prot_wrapper));
    ((long*)int8_prot_stack)[0] = 1;
    *(short*)(int8_prot_wrapper + 0x09) = __djgpp_ds_alias;
    *(long*)(int8_prot_wrapper + 0x0f) = (long)int8_prot_stack + 8;
    *(long*)(int8_prot_wrapper + 0x15) = (long)int8_prot_stack + 4;
    *(long*)(int8_prot_wrapper + 0x1e) = (long)int8_prot_stack + 4;
    *(long*)(int8_prot_wrapper + 0x2a) = (long)int8_prot_stack + _go32_interrupt_stack_size;
    *(long*)(int8_prot_wrapper + 0x3a) = (long)_int_8h_prot - ((long)int8_prot_wrapper + 0x3e);

    *(long*)(int8_prot_wrapper + 0x4a) = (long)int8_prot_stack + 4;
    *(long*)(int8_prot_wrapper + 0x5a) = (long)int8_prot_wrapper + 0x5f;
    *(long*)(int8_prot_wrapper + 0x5f) = _timer.handler_prot_old.pm_offset;
    *(short*)(int8_prot_wrapper + 0x63) = _timer.handler_prot_old.pm_selector;

    *(long*)(int8_prot_wrapper + 0x6d) = (long)int8_prot_stack + 4;

    _timer.handler_prot_new.pm_offset = (uint32_t)int8_prot_wrapper;
    _timer.handler_prot_new.pm_selector = (uint16_t)_go32_my_cs();
}

void _timer_switch(bool hires)
{
    if (_timer.installed) {
        disable();
        // https://wiki.osdev.org/Programmable_Interval_Timer
        // PIT command register - channel 0, two bytes, mode 2 (rate generator)
        outportb(0x43, 0x34);
        // PIT0 data register - load in our new frequency
        outportb(0x40, (TIMER_PIT_CLOCK / (hires ? TIMER_HIRES_HZ : TIMER_HZ)) & 0xff);
        outportb(0x40, (TIMER_PIT_CLOCK / (hires ? TIMER_HIRES_HZ : TIMER_HZ)) >> 8);
        log_print("dos:_timer_switch: DOS timer hotpatched to run at %dHz\n", hires ? TIMER_HIRES_HZ : TIMER_HZ);
        _timer.hires = hires;
        enable();
    }
}

void timer_shutdown();

void timer_init()
{
    // DOS - get dos version
    _go32_dpmi_registers r;
    r.h.ah = 0x30;
    r.h.al = 0x00;
    _go32_dpmi_simulate_int(0x21, &r);
    errno = 0;
    // Check if we're running with an external DPMI server (e.g. Windows 98)
    __dpmi_yield();
    if (errno != ENOSYS) {
        use_dpmi_yield = true;
    }
    bool use_hires = false; //! use_dpmi_yield;

    log_print("dos:timer_init: DOS version %d.%02d\n", r.h.al, r.h.ah);
    log_print("dos:timer_init: %s DPMI server detected\n", use_dpmi_yield ? "Windows" : "Bare metal");

    if (__djgpp_nearptr_enable() == 0) {
        log_print("Couldn't access the first 640K of memory. Boourns.\n");
        exit(-1);
    }

    if (!_timer.installed) {
        log_print("dos:timer_init: Replacing DOS timer interrupt...\n");
        // Disable all interrupts while installing the new timer
        disable();

        // set up measurement constants
        _timer_freq = TIMER_HZ;
        _timer_tick_per_ms = _timer_freq / 1000;
        _timer_ms_per_tick = 1000 / _timer_freq;
        _timer.installed = true;

        // zero counters
        _timer.ticks = 0;
        _timer.hiresticks = 0;
        _timer.oldticks = 0;
        _timer.counter = 0;
        _timer.reset = (int16_t)(TIMER_HZ / TIMER_DEFAULT_FREQ);

        // Lock data memory used by timer state
        LOCK_DATA(_timer)

        // Replace the protected mode INT 8H handler.
        _go32_dpmi_get_protected_mode_interrupt_vector(8, &_timer.handler_prot_old);
        _setup_int8_wrapper();
        _go32_dpmi_set_protected_mode_interrupt_vector(8, &_timer.handler_prot_new);

        // We don't need to replace the real mode INT 8H handler.
        // As far as I can tell, DJGPP's default real mode handler will
        // try and switch to protected mode and call the protected mode handler
        // to do the work. AKA the thing we just set up.

        // All done, interrupts back on.
        enable();

        // Cank the timer frequency.
        // If DPMI is present, 99% chance we're in Windows, so don't go to 16KHz.
        _timer_switch(use_hires);

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
        dos_yield();
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
        free(int8_prot_stack);
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
        log_print("dos:timer_shutdown: %d old ticks, %d millis\n", _timer.oldticks, _timer.ticks);
    }
}

void timer_print()
{
    log_print("timer_print: %d slots\n", _timer.slot_head);
    for (int i = 0; i < _timer.slot_head; i++) {
        log_print("[%d] id: %d, count: %d, interval: %d, callback: %p\n", i, _timer.slots[i].id, _timer.slots[i].count,
            _timer.slots[i].interval, (void*)_timer.slots[i].callback);
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
    log_print("timer_add_callback: Adding timer id %d to slot %d\n", _timer.max_callback_id, _timer.slot_head);
    _timer.slot_head++;
    timer_print();
    return _timer.max_callback_id;
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
            log_print("timer_remove_callback: Removing timer id %d from slot %d\n", id, i);
            timer_print();
            result = true;
            break;
        }
    }
    return result;
}

pt_drv_timer dos_timer = { &timer_init, &timer_shutdown, &timer_ticks, &timer_millis, &timer_sleep, &timer_add_callback,
    &timer_remove_callback };

void pcspeaker_play_sample(byte* data, size_t len, int rate);
void pcspeaker_stop();
void pcspeaker_init()
{
    // 6-bit LUT for the sample playback mode
    for (int i = 0; i < 256; i++) {
        pcspeaker_lut[i] = (uint8_t)(i * TIMER_PIT_CLOCK / (TIMER_HIRES_HZ * 255));
    }
    memset(&_pcspeaker, 0, sizeof(_pcspeaker));
    LOCK_DATA(_pcspeaker);
    pcspeaker_stop();
}

void pcspeaker_shutdown()
{
    pcspeaker_stop();
}

void pcspeaker_tone(float freq)
{
    _pcspeaker.mode = PCSPEAKER_TONE;
    // PIT mode/command register:
    // - counter 2 (PC speaker)
    // - bits 0-7 then 8-15
    // - mode 3 (square wave generator)
    outportb(0x43, 0xb6);
    // Load tone frequency into PIT channel 2 data port
    // The PC speaker is attached to PIT channel 2 and has two states: high
    // and low. The square wave generator will cycle between high and low
    // after a specified number of ticks of the main clock (1193180Hz),
    // making a tone frequency at the loudest possible volume.
    uint32_t units = TIMER_PIT_CLOCK / freq;
    outportb(0x42, units & 0xff);
    outportb(0x42, units >> 8);
    // Enable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) | 3);
}

void pcspeaker_play_sample(byte* data, size_t len, int rate)
{
    if (!_timer.hires) {
        log_print("pcspeaker_sample: hires timer not enabled, sample playback disabled\n");
        return;
    } else if (rate == 8000) {
        _pcspeaker.mode = PCSPEAKER_SAMPLE_8K;
    } else if (rate == 16000) {
        _pcspeaker.mode = PCSPEAKER_SAMPLE_16K;
    } else {
        log_print("pcspeaker_sample: audio must have a sample rate of 8000 or 16000\n");
        return;
    }
    _pcspeaker.sample_data = data;
    _pcspeaker.sample_len = len;
    _pcspeaker.offset = 0;
    _go32_dpmi_lock_data(data, len);
    // PIT mode/command register:
    // - counter 2 (PC speaker)
    // - bits 0-7 only
    // - mode 0 (interrupt on terminal count)
    outportb(0x43, 0x90);
    // Enable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) | 0x3);
}

void pcspeaker_sample_update()
{
    // This is the same trick used by Access Software's "RealSound".
    // Even though we can only send an on-off signal to the PC speaker,
    // the speaker cone is still subject to real world physics.
    // If we change the signal faster than the cone can move, then
    // it will average out to an analogue waveform.

    // We set PIT channel 2 to mode 0; this means whenever we write [value]
    // to counter 2, the PC speaker output will go high for [value] main
    // clock ticks, then go low.

    // ------------0x4a------------
    // --- value ----
    // _____________
    //              |______________ -> PC speaker

    // Say that we ran our main timer at 16000Hz. That gives us 0x4a ticks
    // of the main clock between each timer interrupt. So if we had an
    // 8000Hz unsigned 8-bit WAV, we could convert each sample to the 0x00-0x4a
    // range, and make our main timer interrupt set counter 2 (twice per sample)
    // to play it back.
    size_t offset = 0;
    if (_pcspeaker.mode == PCSPEAKER_SAMPLE_8K) {
        offset = _pcspeaker.offset >> 1;
    } else if (_pcspeaker.mode == PCSPEAKER_SAMPLE_16K) {
        offset = _pcspeaker.offset;
    } else {
        return;
    }
    if (offset > _pcspeaker.sample_len) {
        pcspeaker_stop();
    } else {
        // PIT channel 2 data port
        // outportb(0x42, _pcspeaker.sample_data[offset]);
        outportb(0x42, pcspeaker_lut[_pcspeaker.sample_data[offset]]);
        _pcspeaker.offset += 1;
    }
}

void pcspeaker_stop()
{
    _pcspeaker.mode = PCSPEAKER_OFF;
    // Disable PC speaker gate on keyboard controller
    outportb(0x61, inportb(0x61) & 0xfc);
}

pt_drv_beep dos_beep = {
    &pcspeaker_init,
    &pcspeaker_shutdown,
    &pcspeaker_tone,
    &pcspeaker_play_sample,
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
}

bool serial_rx_ready();
byte serial_getc();
void serial_close_device();

void serial_open_device(const char* device)
{
    if (device) {
        if (strcmp(device, "COM1") == 0) {
            serial_port_base = COM1_PORT;
        } else if (strcmp(device, "COM2") == 0) {
            serial_port_base = COM2_PORT;
        } else if (strcmp(device, "COM3") == 0) {
            serial_port_base = COM3_PORT;
        } else if (strcmp(device, "COM4") == 0) {
            serial_port_base = COM4_PORT;
        } else {
            log_print("serial_open_device: Unknown device %s, ignoring\n", device);
        }
    }

    if (!serial_port_base)
        return;

    // COM4 - FIFO Control Register
    // - Clear Transmit FIFO
    // - Clear Receive FIFO
    // - Enable FIFO
    outportb(serial_port_base + SERIAL_IIR_FCR, 0x07);

    // COM4 - Interrupt Identification Register
    // Check for a 16550; FIFO enabled bits
    byte typecheck = inportb(serial_port_base + SERIAL_IIR_FCR);
    if ((typecheck & 0xf0) != 0xc0) {
        log_print("serial_open_device: 16550 UART not found for %s (%02x), serial disabled\n", device, typecheck);
        serial_port_base = 0;
        return;
    }

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

    // If there's nothing attached to the COM port, bail
    if (serial_rx_ready() && serial_getc() == 0xff) {
        log_print("serial_open_device: nothing on the other end of %s, serial disabled\n", device);
        serial_close_device();
        return;
    }
    log_print("serial_open_device: listening on UART %s\n", device);
}

void serial_close_device()
{
    if (!serial_port_base)
        return;

    // COM4 - Modem Control Register
    // Zero out
    outportb(serial_port_base + SERIAL_MCR, 0);

    // COM4 - FIFO Control Register
    // Zero out
    outportb(serial_port_base + SERIAL_IIR_FCR, 0);

    serial_port_base = 0;
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
            dos_yield();
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
        dos_yield();
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
    serial_close_device();
}

pt_drv_serial dos_serial = {
    &serial_init,
    &serial_shutdown,
    &serial_open_device,
    &serial_close_device,
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
    _timer.real_mode_compat = true;
    int86(0x33, &regs, &regs);
    _timer.real_mode_compat = false;
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
    _timer.real_mode_compat = true;
    int86(0x33, &regs, &regs);
    _timer.real_mode_compat = false;
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
            _timer.real_mode_compat = true;
            int86(0x33, &regs, &regs);
            _timer.real_mode_compat = false;

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
    [84] = "?",       [85] = "?",         [86] = "?",          [87] = "f11",
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
}

void keyboard_init()
{
    // Disable all interrupts while installing the new keyboard interrupt
    disable();

    // Zero out the key event buffer
    memset(&keyevent_buffer, 0, sizeof(keyevent_buffer));

    _go32_dpmi_lock_data((char*)&keyboard_key_states, KEY_MAX);
    LOCK_DATA(keyboard_flags)
    LOCK_DATA(keyevent_buffer)

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

void _opl_lock();
void opl_shutdown();

// detection method nicked from https://moddingwiki.shikadi.net/wiki/OPL_chip#Detection_Methods
void opl_init()
{
    _opl_lock();

    log_print("dos:opl_init: Detecting Yamaha OPL chip...\n");

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
    // zero all the registers
    for (int i = 1; i < 0xf6; i++) {
        sound_opl3_out_raw(OPL3_BASE_PORT, i, 0);
        sound_opl3_out_raw(OPL3_BASE_PORT + 2, i, 0);
    }
    sound_opl2_available = true;
    if ((status2 & 0x06) != 0) {
        log_print("dos:opl_init: found Yamaha OPL2\n");
    } else {
        sound_opl3_available = true;
        log_print("dos:opl_init: found Yamaha OPL3\n");
    }
    atexit(opl_shutdown);
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

void _opl_lock()
{ // Lock the memory page that contains all of the OPL code
    LOCK_DATA(sound_opl2_available) LOCK_DATA(sound_opl3_available)
}

bool opl_is_ready()
{
    return sound_opl2_available || sound_opl3_available;
}

pt_drv_opl dos_opl = { &opl_init, &opl_shutdown, &opl_write_reg, &opl_is_ready };

void dos_init()
{
    // We're running under CWSDPMI in protected mode.
    // That means virtual memory. On the surface it looks like
    // one big continuous address space, but underneath its based
    // on 4K pages which can be shuffled between RAM and a
    // swap file on the disk.

    // This causes a problem with interrupt code:
    // - interrupts can be raised at -any time- during execution
    // - interrupts can't load pages from the swap file
    // - ergo, everything an interrupt touches has to be in RAM

    // DPMI advises you to lock all regions code or data memory
    // that are used by interrupts. For us, that's the driver layer,
    // plus anything accessed by the timer callback system.

    // Lock data so that pt_sys is accessible from interrupts.
    // All of these memory regions are statically declared,
    // so this macro uses sizeof() to find the end.
    LOCK_DATA(pt_sys)
    LOCK_DATA(dos_app)
    LOCK_DATA(dos_timer)
    LOCK_DATA(dos_keyboard)
    LOCK_DATA(dos_mouse)
    LOCK_DATA(dos_serial)
    LOCK_DATA(dos_opl)
    LOCK_DATA(dos_beep)
    LOCK_DATA(dos_vga)

    // As it turns out, there's no guarantee with new GCC
    // that functions will be linked and stored in the same
    // order, and therefore no straightforward way of getting the
    // memory range of individual functions for locking.
    // Most DJGPP advice will tell you to do something like:
    //
    // void func() { ... }
    // void func_end() {}
    //
    // int main(int argc, char **argv) {
    //     _go32_dpmi_lock_code((void *)func, (long)func_end - (long)func);
    // }
    //
    // which will not work if you use modern GCC with the default
    // optimisation settings, as func_end() is not guaranteed to be
    // positioned by the linker after func().

    // gamedevjeff points out this was introduced by gcc in about 2007
    // for -O1 and above, and supposedly can be disabled with
    // -fno-toplevel-reorder.

    // We could use pragma sections to mask off areas of code, if it
    // weren't for the fact that CWSDPMI is hardcoded to load .text and
    // .data and nothing else.

    // For now, just lock ALL of .text using the magic GCC linker symbol
    // for the end of the .text area.
    // As of writing that's 93 pages. Peanuts for a Pentium. Plenty of
    // other junk that deserves swapping before that.
    _go32_dpmi_lock_code((void*)0x1000, (long)&etext - (long)0x1000);
}

void dos_set_meta(const char* name, const char* version, const char* identifier)
{
}

char* dos_get_data_path()
{
    char* buffer = (char*)calloc(512, sizeof(char));
    getcwd(buffer, 512);
    size_t idx = strlen(buffer);
    // DJGPP uses pretendy UNIX paths. Make sure it ends with a slash.
    if (idx < 512 && buffer[idx - 1] != '/')
        buffer[idx] = '/';
    return buffer;
}

void dos_shutdown()
{
}

pt_drv_app dos_app = { &dos_init, &dos_set_meta, &dos_get_data_path, &dos_shutdown };
