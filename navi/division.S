/* GCC's SH division helper has a narrower clobber set than a C function.
 * Preserve every other call-clobbered integer register, returning only r0.
 * The C implementations themselves contain no division operations.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
    .section .text,"ax"
    .align 2
    .global __sdivsi3_i4i
    .type __sdivsi3_i4i,@function
__sdivsi3_i4i:
    mov.l r1,@-r15
    mov.l r2,@-r15
    mov.l r3,@-r15
    mov.l r4,@-r15
    mov.l r5,@-r15
    mov.l r6,@-r15
    mov.l r7,@-r15
    sts.l pr,@-r15
    mov.l .Lsigned,r0
    jsr @r0
    nop
    bra .Lrestore
    nop
    .align 2
.Lsigned: .long __divsi3
    .size __sdivsi3_i4i,.-__sdivsi3_i4i

    .align 2
    .global __udivsi3_i4i
    .type __udivsi3_i4i,@function
__udivsi3_i4i:
    mov.l r1,@-r15
    mov.l r2,@-r15
    mov.l r3,@-r15
    mov.l r4,@-r15
    mov.l r5,@-r15
    mov.l r6,@-r15
    mov.l r7,@-r15
    sts.l pr,@-r15
    mov.l .Lunsigned,r0
    jsr @r0
    nop
.Lrestore:
    lds.l @r15+,pr
    mov.l @r15+,r7
    mov.l @r15+,r6
    mov.l @r15+,r5
    mov.l @r15+,r4
    mov.l @r15+,r3
    mov.l @r15+,r2
    mov.l @r15+,r1
    rts
    nop
    .align 2
.Lunsigned: .long __udivsi3
    .size __udivsi3_i4i,.-__udivsi3_i4i
    .section .note.GNU-stack,"",@progbits
