#include <stdio.h>

#include "timer.h"
#include "io.h"
#include "fifo.h"
#include "int.h"

struct TimerCtl timerctl;

void init_pit(void) {
    io_out8(PIT_CTRL, 0x34);
    io_out8(PIT_CNT0, 0x9c);
    io_out8(PIT_CNT0, 0x2e);
    timerctl.count = 0;
    timerctl.next = 0xffffffff;
    
    for (int i = 0; i < MAX_TIMER; i++) {
        timerctl.timers0[i].flags = 0;
    }
    return;
}

void int_handler20(int *esp) {
    io_out8(PIC0_OCW2, 0x60);
    timerctl.count++;

    if (timerctl.next > timerctl.count) {
        return;
    }

    int i, j;
    for (i = 0; i < timerctl.using; i++) {
        if (timerctl.timers[i]->timeout > timerctl.count) {
            break;
        }

        timerctl.timers[i]->flags = TIMER_FLAGS_ALLOC;
        fifo8_put(timerctl.timers[i]->fifo, timerctl.timers[i]->data);
    }

    timerctl.using -= i;

    for (j = 0; j < timerctl.using; j++) {
        timerctl.timers[j] = timerctl.timers[i + j];
    }
    if (timerctl.using > 0) {
        timerctl.next = timerctl.timers[0]->timeout;
    } else {
        timerctl.next = 0xffffffff;
    }

    return;
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

void timer_init(struct Timer *timer, struct FIFO8 *fifo, unsigned char data) {
    timer->fifo = fifo;
    timer->data = data;
    return;
}

void timer_set_timer(struct Timer *timer, unsigned int timeout) {
    timer->timeout = timeout + timerctl.count;
    timer->flags = TIMER_FLAGS_USING;

    int i, j;
    int e = io_load_eflags();
    io_cli();

    for (i = 0; i < timerctl.using; i++) {
        if (timerctl.timers[i]->timeout >= timer->timeout) {
            break;
        }
    }

    for (j = timerctl.using; j > i; j--) {
        timerctl.timers[j] = timerctl.timers[j - 1];
    }
    timerctl.using++;

    timerctl.timers[i] = timer;
    timerctl.next = timerctl.timers[0]->timeout;
    io_store_eflags(e);
}