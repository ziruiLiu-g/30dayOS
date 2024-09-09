#ifndef _TASK_H_
#define _TASK_H_

extern struct Timer *mt_timer;
extern int mt_tr;

// 104 bytes
struct TSS32 {
  int backlink, esp0, ss0, esp1, ss1, esp2, ss2, cr3;
  int eip, eflags, eax, ecx, edx, ebx, esp, ebp, esi, edi;
  int es, cs, ss, ds, fs, gs;
  int ldtr, iomap;
};

void load_tr(int tr);
void farjmp(int eip, int cs);
void mt_init(void);
void mt_task_switch(void);

#endif // _TASK_H_
