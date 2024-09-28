#ifndef _TASK_H_
#define _TASK_H_

#include "memory.h"
#include "timer.h"

#define MAX_TASKS 1000
#define TASK_GDT0 3    // 定义从GDT的几号开始分配给TSS

#define MAX_TASKS_LV  100
#define MAX_TASKLEVELS 10

// 104 bytes
struct TSS32 {
  int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
  int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
  int es, cs, ss, ds, fs, gs;
  int ldtr, iomap;
};

struct FIFO32;

struct Task {
  int sel, flags;   // sel for gdt number
  int priority, level;
  struct FIFO32 fifo;
  struct TSS32 tss;
};

struct TaskLevel {
  int running; // total number of running tasks
  int now; // the number(id) of current running task
  struct Task *tasks[MAX_TASKS_LV];
};

struct TaskCtl {
  int now_lv;
  int lv_change; // if level need to be changed in next switch
  struct TaskLevel level[MAX_TASKLEVELS];
  struct Task tasks0[MAX_TASKS];
};

extern struct Timer *task_timer;
extern struct TaskCtl *taskctl;

void load_tr(int tr);
void far_jmp(int eip, int cs);
void far_call(int eip, int cs);
void task_switch(void);

struct Task *task_init(struct MemMan *memman);
struct Task *task_alloc(void);
void task_run(struct Task *task, int level, int priority);
void task_sleep(struct Task *task);

struct Task *task_now(void);
void task_add(struct Task *task);
void task_remove(struct Task *task);
void task_switchsub(void);

void task_idle(void);

#endif // _TASK_H_
