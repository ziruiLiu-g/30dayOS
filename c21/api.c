#include "api.h"
#include "console.h"
#include "task.h"

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx,
             int eax) {
  struct Console *cons = (struct Console *)*((int *) 0x0fec);
  int cs_base = *((int *) 0xfe8);
  struct Task *task = task_now();

  if (edx == 1) {
    cons_putchar(cons, eax & 0xff, 1);
  } else if (edx == 2) {
    cons_putstr(cons, (char *)ebx + cs_base);
  } else if (edx == 3) {
    cons_putnstr(cons, (char *)ebx + cs_base, ecx);
  } else if (edx == 4) {
    return &(task->tss.esp0);
  }

  return 0;
}
