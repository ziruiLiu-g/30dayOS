#include "desctbl.h"
#include "int.h"
#include "api.h"

void init_gdtidt(void) {
  struct SegmentDescriptor *gdt = (struct SegmentDescriptor *) 0x00270000;
  struct GateDescriptor *idt = (struct GateDescriptor *) 0x0026f800;

  int i;

  for (i = 0; i < 8192; i++) {
    set_segmdesc(gdt + i, 0, 0, 0);
  }
  set_segmdesc(gdt + 1, 0xffffffff, 0x00000000, AR_DATA32_RW);
  set_segmdesc(gdt + 2, LIMIT_BOOTPACK, ADR_BOOTPACK, AR_CODE32_ER);
  load_gdtr(LIMIT_GDT, ADR_GDT);

  for (int i = 0; i <= LIMIT_IDT / 8; i++) {
    set_gatedesc(idt + i, 0, 0, 0);
  }
  load_idtr(LIMIT_IDT, ADR_IDT);

  set_gatedesc(idt + 0x0c, (int) asm_int_handler0c, 2 * 8, AR_INTGATE32);
  set_gatedesc(idt + 0x0d, (int) asm_int_handler0d, 2 * 8, AR_INTGATE32);

  set_gatedesc(idt + 0x20, (int) asm_int_handler20, 2 * 8, AR_INTGATE32);
  set_gatedesc(idt + 0x21, (int) asm_int_handler21, 2 * 8, AR_INTGATE32);
  set_gatedesc(idt + 0x27, (int) asm_int_handler27, 2 * 8, AR_INTGATE32);
  set_gatedesc(idt + 0x2c, (int) asm_int_handler2c, 2 * 8, AR_INTGATE32);
  set_gatedesc(idt + 0x40, (int) asm_hrb_api, 2 * 8, AR_INTGATE32 + 0x60);

  return;
}

void set_segmdesc(struct SegmentDescriptor *sd, unsigned int limit, int base, int ar) {
  if (limit > 0xfffff) {
    ar |= 0x8000;
    limit /= 0x1000;
  }

  sd->limit_low = limit & 0xffff;
  sd->base_low = base & 0xffff;
  sd->base_mid = (base >> 16) & 0xff;
  sd->access_right = ar & 0xff;
  sd->limit_high = ((limit >> 16) & 0x0f) | ((ar >> 8) | 0xf0);
  sd->base_high = (base >> 24) & 0xff;
}

void set_gatedesc(struct GateDescriptor *gd, int offset, int selector, int ar) {
  gd->offset_low = offset & 0xffff;
  gd->selector = selector;
  gd->dw_count = (ar >> 8) & 0xff;
  gd->access_right = ar & 0xff;
  gd->offset_high = (offset >> 16) & 0xffff;
}