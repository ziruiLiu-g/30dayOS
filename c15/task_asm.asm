  [BITS 32]

  GLOBAL load_tr, farjmp

load_tr:          ; void load_tr(int tr);
  LTR   [ESP+4]   ; tr
  RET

farjmp:           ; void farjmp(int eip, int cs)
  JMP   FAR [ESP + 4]
  RET