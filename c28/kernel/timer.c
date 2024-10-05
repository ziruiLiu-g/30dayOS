#include <stdio.h>

#include "timer.h"
#include "io.h"
#include "int.h"
#include "task.h"

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
    char ts = 0;

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
        if (timer != task_timer) {
            fifo32_put(timer->fifo, timer->data);
        } else {
            ts = 1;
        }
        timer = timer->next;
    }

    timerctl.t0 = timer;
    timerctl.next = timerctl.t0->timeout;

    if (ts != 0) {
        task_switch();
    }
}

struct Timer *timer_alloc(void) {
    int i;
    for (i = 0; i < MAX_TIMER; i++) {
        if (timerctl.timers0[i].flags == 0) {
            timerctl.timers0[i].flags = TIMER_FLAGS_ALLOC;
            timerctl.timers0[i].flags2 = 0;
            return &timerctl.timers0[i];
        }
    }

    return 0;
}

void timer_free(struct Timer *timer) {
    timer->flags = 0;
    return;
}

void timer_init(struct Timer *timer, struct FIFO32 *fifo, int data) {
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

        if (timer->timeout <= t->timeout) {
            s->next = timer;
            timer->next = t;
            io_store_eflags(e);
            return;
        }
    }
}

int timer_cancel(struct Timer *timer) {
    struct Timer *t;
  int eflags = io_load_eflags();
  io_cli();

  if (timer->flags == TIMER_FLAGS_USING) {
    if (timer == timerctl.t0) {
      t = timer->next;

      timerctl.t0 = t;
      timerctl.next = t->timeout;
    } else {
      t = timerctl.t0;
      for (;;) {
        if (t->next == timer) {
          break;
        }
        t = t->next;
      }
      t->next = timer->next;
    }

    timer->flags = TIMER_FLAGS_ALLOC;
    io_store_eflags(eflags);
    return 1;
  }

  io_store_eflags(eflags);
  return 0;
}

void timer_cancel_all(struct FIFO32 *fifo) {
    int eflags = io_load_eflags();
    io_cli();

    for (int i = 0; i < MAX_TIMER; i++) {
        struct Timer *t = &timerctl.timers0[i];
        if (t->flags != 0 && t->flags2 != 0 && t->fifo == fifo) {
        timer_cancel(t);
        timer_free(t);
        }
    }

    io_store_eflags(eflags);
}