    .section .text.entry,"ax"
    .global _start
_start:
    mov.l .Lfpscr,r0
    lds r0,fpscr
    mov.l .Lbss,r1
    mov.l .Lbssend,r2
    mov #0,r0
1:  cmp/hs r2,r1
    bt 2f
    mov.l r0,@r1
    bra 1b
    add #4,r1
2:  mov.l .Lmain,r1
    jsr @r1
    nop
3:  bra 3b
    nop
    .align 2
.Lfpscr: .long 0x000c0001
.Lbss: .long __bss_start
.Lbssend: .long __bss_end
.Lmain: .long navi_main
    .section .note.GNU-stack,"",@progbits
