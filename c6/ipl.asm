; hello-os

CYLS EQU 10
    ORG 0x7c00      ; program load address

; For FAT12 Format Disk
    JMP entry
    NOP
    DB  "HARIBOTE"
    DW  512
    DB  1
    DW  1
    DB  2
    DW  224
    DW  2880
    DB  0xf0
    DW  9
    DW  18
    DW  2
    DD  0
    DD  2880
    DB  0
    DB  0
    DB  0x29
    DD  0xffffffff
    DB  "HARIBOTEOS "
    DB  "FAT12   "
    RESB 18

; core logic

entry:
    MOV AX, 0       ; initiate reg
    MOV SS, AX
    MOV SP, 0x7c00
    MOV DS, AX

    ; Read Driver
    MOV AX,0x0820   ; 0x8000 ~ 0x81ff are reserved
    MOV ES, AX
    MOV CH, 0
    MOV DH, 0
    MOV CL, 2

readloop:
    MOV SI, 0

retry:
    MOV AH, 0x02    ; 0x02 -> READ
    MOV AL, 1       ; 1 FAN
    MOV BX, 0       ;
    MOV DL, 0x00    ; Driver A
    INT 0x13
    JNC next        ; jump if not carry
    
    ADD SI, 1
    CMP SI, 5
    JAE error
    MOV AH, 0x00
    MOV DL, 0x00
    INT 0x13
    JMP retry

next:
    MOV AX, ES
    ADD AX, 0x0020  ; 0x0020
    MOV ES, AX

    ADD CL, 1
    CMP CL, 18
    JBE readloop
    MOV CL, 1
    ADD DH, 1
    CMP DH, 2
    JB  readloop
    MOV DH, 0
    ADD CH, 1
    CMP CH, CYLS
    JB  readloop

    MOV   [0x0ff0], CH      ;
    JMP   0xc200

putloop:
    MOV AL, [SI]
    ADD SI, 1
    CMP AL, 0

    JE fin
    MOV AH, 0x0e    ; to show one char
    MOV BX, 15      ; set char color
    INT 0X10        ; VIDEO CARD BIOS
    JMP putloop

fin:
    HLT
    JMP fin

error:
    MOV SI, msg

msg:
    DB  0x0a, 0x0a
    DB  "hello, world"
    DB  0x0a
    DB  0

    RESB    0x1fe - ($ - $$)
    DB  0x55, 0xaa