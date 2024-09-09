#include <stdio.h>

#include "timer.h"
#include "io.h"
#include "fifo.h"
#include "int.h"

struct TimerCtl timerctl;

void init_pit(void) {
    int i;
    struct Timer *t;

    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;

    for (i = 0; i < MAX_TIMER; i++) {
        timerctl.timers0[i].flags = 0;
    }
    t = timer_alloc();
    t->timeout = 0xffffffff;
    t->flags = TIMER_FLAGS_USING;
    t->next = 0;
    timerctl.t0 = t;
    timerctl.next = 0xffffffff;
    return;
}

void int_handler20(int *esp) {
    int i;
    struct Timer *timer;
    io_out8(PIC0_OCW2, 0x60);
    timerctl.count++;
    if (timerctl.next > timerctl.count) {
        return;
    }

    timer = timerctl.t0;
    for (;;) {
        if (timer->timeout > timerctl.count) {
            break;
        }

        /* timeout */
        timer->flags = TIMER_FLAGS_ALLOC;
        fifo32_put(timer->fifo, timer->data);
        timer = timer->next;
    }

    timerctl.t0 = timer;
    timerctl.next = timerctl.t0->timeout;
}

struct Timer *timer_alloc(void) {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timerctl.timers0[i].flags == 0) {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            return &timerctl.timers0[i];
        }
    }

    return 0;
}

void timer_free(struct Timer *timer) {
    timer->flags = 0;
    return;
}

void timer_init(struct Timer *timer, struct FIFO32 *fifo, unsigned char data) {
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_set_timer(struct Timer *timer, unsigned int timeout) {
    int e;
    struct Timer *t, *s;

    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;

    e = io_load_eflags();
    io_cli();

    t = timerctl.t0;
    if (timer->timeout <= t->timeout) {
        timerctl.t0 = timer;
        timer->next = t;
        timerctl.next = timer->timeout;
        io_store_eflags(e);
        return;
    }
    for(;;) {
        s = t;
        t = t->next;
        if (t == 0) {
            break;
        }

        if (timer->timeout <= t->timeout) {
            s->next = timer;
            timer->next = t;
            io_store_eflags(e);
            return;
        }
    }
}