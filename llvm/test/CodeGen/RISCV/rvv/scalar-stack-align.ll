; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=riscv32 -mattr=+zve64x -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=RV32
; RUN: llc -mtriple=riscv64 -mattr=+zve64x -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=RV64
; RUN: llc -mtriple=riscv32 -mattr=+v -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=RV32
; RUN: llc -mtriple=riscv64 -mattr=+v -verify-machineinstrs < %s \
; RUN:   | FileCheck %s --check-prefix=RV64

; FIXME: We are over-aligning the stack on V, wasting stack space.

define ptr @scalar_stack_align16() nounwind {
; RV32-LABEL: scalar_stack_align16:
; RV32:       # %bb.0:
; RV32-NEXT:    addi sp, sp, -48
; RV32-NEXT:    sw ra, 44(sp) # 4-byte Folded Spill
; RV32-NEXT:    csrr a0, vlenb
; RV32-NEXT:    slli a0, a0, 1
; RV32-NEXT:    sub sp, sp, a0
; RV32-NEXT:    addi a0, sp, 32
; RV32-NEXT:    call extern
; RV32-NEXT:    addi a0, sp, 16
; RV32-NEXT:    csrr a1, vlenb
; RV32-NEXT:    slli a1, a1, 1
; RV32-NEXT:    add sp, sp, a1
; RV32-NEXT:    lw ra, 44(sp) # 4-byte Folded Reload
; RV32-NEXT:    addi sp, sp, 48
; RV32-NEXT:    ret
;
; RV64-LABEL: scalar_stack_align16:
; RV64:       # %bb.0:
; RV64-NEXT:    addi sp, sp, -48
; RV64-NEXT:    sd ra, 40(sp) # 8-byte Folded Spill
; RV64-NEXT:    csrr a0, vlenb
; RV64-NEXT:    slli a0, a0, 1
; RV64-NEXT:    sub sp, sp, a0
; RV64-NEXT:    addi a0, sp, 32
; RV64-NEXT:    call extern
; RV64-NEXT:    addi a0, sp, 16
; RV64-NEXT:    csrr a1, vlenb
; RV64-NEXT:    slli a1, a1, 1
; RV64-NEXT:    add sp, sp, a1
; RV64-NEXT:    ld ra, 40(sp) # 8-byte Folded Reload
; RV64-NEXT:    addi sp, sp, 48
; RV64-NEXT:    ret
  %a = alloca <vscale x 2 x i32>
  %c = alloca i64, align 16
  call void @extern(<vscale x 2 x i32>* %a)
  ret ptr %c
}

declare void @extern(<vscale x 2 x i32>*)
