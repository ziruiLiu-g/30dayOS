#include "task.h"
#include "timer.h"

struct Timer *mt_timer;
int mt_tr;

void mt_init(void) {
    mt_timer = timer_alloc();
    timer_set_timer(mt_timer, 2);
    mt_tr = 3 * 8;
    return;
}

void mt_task_switch(void) {
    if (mt_tr == 3 * 8) {
        mt_tr = 4 * 8;
    } else {
        mt_tr = 3 * 8;
    }

    timer_set_timer(mt_timer, 2);
    farjmp(0, mt_tr);
    return;
}