;
; FastBasic - Fast basic interpreter for the Atari 8-bit computers
; Copyright (C) 2017,2018 Daniel Serpell
;
; This program is free software; you can redistribute it and/or modify
; it under the terms of the GNU General Public License as published by
; the Free Software Foundation, either version 2 of the License, or
; (at your option) any later version.
;
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
;
; You should have received a copy of the GNU General Public License along
; with this program.  If not, see <http://www.gnu.org/licenses/>
;
; In addition to the permissions in the GNU General Public License, the
; authors give you unlimited permission to link the compiled version of
; this file into combinations with other programs, and to distribute those
; combinations without any restriction coming from the use of this file.
; (The General Public License restrictions do apply in other respects; for
; example, they cover modification of the file, and distribution when not
; linked into a combine executable.)


; Decrement a memory location
; ---------------------------

        ; From interpreter.asm
        .import         pop_stack_y

        .segment        "RUNTIME"

.proc   EXE_DEC ; DPOKE(AX, DPEEK(AX) - 1) ; POP_AX
        stx     loadH+2
        stx     loadL1+2
        stx     loadL2+2
        tax
loadL1: lda     $FF00, x
        bne     loadL2
loadH:  dec     $FF01, x
loadL2: dec     $FF00, x
        jmp     pop_stack_y
.endproc

        .include "../deftok.inc"
        deftoken "DEC"

; vi:syntax=asm_ca65
