#include <stdio.h>

#include "task.h"
#include "timer.h"
#include "memory.h"
#include "desctbl.h"

struct TaskCtl *taskctl;
struct Timer *task_timer;

void task_switch(void) {
    struct TaskLevel *tl = &taskctl->level[taskctl->now_lv];
    struct Task *new_task, *now_task = tl->tasks[tl->now];

    tl->now++;

    if (tl->now == tl->running) {
        tl->now = 0;
    }

    if (taskctl->lv_change != 0) {
        task_switchsub();
        tl = &taskctl->level[taskctl->now_lv];
    }

    new_task = tl->tasks[tl->now];
    timer_set_timer(task_timer, new_task->priority);
    if (new_task != now_task) {
        far_jmp(0, new_task->sel);
    }
}

struct Task *task_init(struct MemMan *memman) {
    int i = 0;
    struct Task *task;
    struct SegmentDescriptor *gdt = (struct SegmentDescriptor *) ADR_GDT;
    taskctl = (struct TaskCtl *) memman_alloc_4k(memman, sizeof(struct TaskCtl));

    for (int i = 0; i < MAX_TASKS; i++) {
        taskctl->tasks0[i].flags = 0;
        taskctl->tasks0[i].sel = (TASK_GDT0 + i) * 8;
        set_segmdesc(gdt + TASK_GDT0 + i, 103, (int)&taskctl->tasks0[i].tss,
                    AR_TSS32);
    }

    for (i = 0; i < MAX_TASKLEVELS; i++) {
        taskctl->level[i].running = 0;
        taskctl->level[i].now = 0;
    }

    task = task_alloc();
    task->flags = 2; // active
    task->priority = 2;     // 0.02 sec
    task->level = 0; // highest

    task_add(task);
    task_switchsub();   // init level
    load_tr(task->sel);
    task_timer = timer_alloc();
    timer_set_timer(task_timer, task->priority);
    return task;
}

struct Task *task_alloc(void) {
    int i;
    struct Task *task;
    for (i = 0; i < MAX_TASKS; i++) {
        if (taskctl->tasks0[i].flags == 0) {
            task = &taskctl->tasks0[i];
            task->flags = 1; // using
            task->tss.eflags = 0x00000202;  /* IF = 1 */
            task->tss.eax = 0;
            task->tss.ecx = 0;
            task->tss.edx = 0;
            task->tss.ebx = 0;
            task->tss.ebp = 0;
            task->tss.esi = 0;
            task->tss.edi = 0;
            task->tss.es = 0;
            task->tss.ds = 0;
            task->tss.fs = 0;
            task->tss.gs = 0;
            task->tss.ldtr = 0;
            task->tss.iomap = 0x40000000;
            return task;
        }
    }
    return 0;
}

void task_run(struct Task *task, int level, int priority) {
    if (level < 0) {
        level = task->level;
    }

    if (priority > 0) {
        task->priority = priority;
    }

    if (task->flags == 2 && task->level != level) {
        task_remove(task);  // after here task->flags will be 1, then the next if statement will be executed;
    }

    if (task->flags != 2) {
        task->level = level;
        task_add(task);
    }

    taskctl->lv_change = 1;
    return;
}

void task_sleep(struct Task *task) {
    struct Task *now_task;

    if (task->flags == 2) {
        now_task = task_now();
        task_remove(task);
        if (task == now_task) {
            task_switchsub();
            now_task = task_now();
            far_jmp(0, now_task->sel);
        }
    }
}

struct Task *task_now(void) {
    struct TaskLevel *tl = &taskctl->level[taskctl->now_lv];
    return tl->tasks[tl->now];
}

void task_add(struct Task *task) {
    struct TaskLevel *tl = &taskctl->level[task->level];
    tl->tasks[tl->running] = task;
    tl->running++;
    task->flags = 2; // active
    return;
}

void task_remove(struct Task *task) {
    int i;
    struct TaskLevel *tl = &taskctl->level[task->level];

    for (i = 0; i < tl->running; i++) {
        if (tl->tasks[i] == task) {
            break;
        }
    }

    tl->running--;

    if (i < tl->now) {
        tl->now--;
    }
    if (tl->now >= tl->running) {
        tl->now = 0;
    }
    task->flags = 1;

    for (; i < tl->running; i++) {
        tl->tasks[i] = tl->tasks[i + 1];
    }
}

void task_switchsub(void) {
    int i;
    for (i = 0; i < MAX_TASKLEVELS; i++) {
        if (taskctl->level[i].running > 0) {
            break;
        }
    }

    taskctl->now_lv = i;
    taskctl->lv_change = 0;
    return;
}