; NOTE: Assertions have been autogenerated by utils/update_mir_test_checks.py UTC_ARGS: --version 3
; RUN: llc -mtriple=riscv64 \
; RUN:     -global-isel -stop-after=irtranslator -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefixes=RV64I,LP64 %s
; RUN: llc -mtriple=riscv64 -mattr=+f -target-abi lp64f \
; RUN:    -global-isel -stop-after=irtranslator -verify-machineinstrs < %s \
; RUN:   | FileCheck -check-prefixes=RV64I,LP64F %s

; This file contains tests that should have identical output for the lp64 and
; lp64f ABIs.

define i64 @callee_double_in_regs(i64 %a, double %b) nounwind {
  ; RV64I-LABEL: name: callee_double_in_regs
  ; RV64I: bb.1 (%ir-block.0):
  ; RV64I-NEXT:   liveins: $x10, $x11
  ; RV64I-NEXT: {{  $}}
  ; RV64I-NEXT:   [[COPY:%[0-9]+]]:_(s64) = COPY $x10
  ; RV64I-NEXT:   [[COPY1:%[0-9]+]]:_(s64) = COPY $x11
  ; RV64I-NEXT:   [[FPTOSI:%[0-9]+]]:_(s64) = G_FPTOSI [[COPY1]](s64)
  ; RV64I-NEXT:   [[ADD:%[0-9]+]]:_(s64) = G_ADD [[COPY]], [[FPTOSI]]
  ; RV64I-NEXT:   $x10 = COPY [[ADD]](s64)
  ; RV64I-NEXT:   PseudoRET implicit $x10
  %b_fptosi = fptosi double %b to i64
  %1 = add i64 %a, %b_fptosi
  ret i64 %1
}

define i64 @caller_double_in_regs() nounwind {
  ; LP64-LABEL: name: caller_double_in_regs
  ; LP64: bb.1 (%ir-block.0):
  ; LP64-NEXT:   [[C:%[0-9]+]]:_(s64) = G_CONSTANT i64 1
  ; LP64-NEXT:   [[C1:%[0-9]+]]:_(s64) = G_FCONSTANT double 2.000000e+00
  ; LP64-NEXT:   ADJCALLSTACKDOWN 0, 0, implicit-def $x2, implicit $x2
  ; LP64-NEXT:   $x10 = COPY [[C]](s64)
  ; LP64-NEXT:   $x11 = COPY [[C1]](s64)
  ; LP64-NEXT:   PseudoCALL target-flags(riscv-call) @callee_double_in_regs, csr_ilp32_lp64, implicit-def $x1, implicit $x10, implicit $x11, implicit-def $x10
  ; LP64-NEXT:   ADJCALLSTACKUP 0, 0, implicit-def $x2, implicit $x2
  ; LP64-NEXT:   [[COPY:%[0-9]+]]:_(s64) = COPY $x10
  ; LP64-NEXT:   $x10 = COPY [[COPY]](s64)
  ; LP64-NEXT:   PseudoRET implicit $x10
  ;
  ; LP64F-LABEL: name: caller_double_in_regs
  ; LP64F: bb.1 (%ir-block.0):
  ; LP64F-NEXT:   [[C:%[0-9]+]]:_(s64) = G_CONSTANT i64 1
  ; LP64F-NEXT:   [[C1:%[0-9]+]]:_(s64) = G_FCONSTANT double 2.000000e+00
  ; LP64F-NEXT:   ADJCALLSTACKDOWN 0, 0, implicit-def $x2, implicit $x2
  ; LP64F-NEXT:   $x10 = COPY [[C]](s64)
  ; LP64F-NEXT:   $x11 = COPY [[C1]](s64)
  ; LP64F-NEXT:   PseudoCALL target-flags(riscv-call) @callee_double_in_regs, csr_ilp32f_lp64f, implicit-def $x1, implicit $x10, implicit $x11, implicit-def $x10
  ; LP64F-NEXT:   ADJCALLSTACKUP 0, 0, implicit-def $x2, implicit $x2
  ; LP64F-NEXT:   [[COPY:%[0-9]+]]:_(s64) = COPY $x10
  ; LP64F-NEXT:   $x10 = COPY [[COPY]](s64)
  ; LP64F-NEXT:   PseudoRET implicit $x10
  %1 = call i64 @callee_double_in_regs(i64 1, double 2.0)
  ret i64 %1
}

define double @callee_double_ret() nounwind {
  ; RV64I-LABEL: name: callee_double_ret
  ; RV64I: bb.1 (%ir-block.0):
  ; RV64I-NEXT:   [[C:%[0-9]+]]:_(s64) = G_FCONSTANT double 1.000000e+00
  ; RV64I-NEXT:   $x10 = COPY [[C]](s64)
  ; RV64I-NEXT:   PseudoRET implicit $x10
  ret double 1.0
}

define i64 @caller_double_ret() nounwind {
  ; LP64-LABEL: name: caller_double_ret
  ; LP64: bb.1 (%ir-block.0):
  ; LP64-NEXT:   ADJCALLSTACKDOWN 0, 0, implicit-def $x2, implicit $x2
  ; LP64-NEXT:   PseudoCALL target-flags(riscv-call) @callee_double_ret, csr_ilp32_lp64, implicit-def $x1, implicit-def $x10
  ; LP64-NEXT:   ADJCALLSTACKUP 0, 0, implicit-def $x2, implicit $x2
  ; LP64-NEXT:   [[COPY:%[0-9]+]]:_(s64) = COPY $x10
  ; LP64-NEXT:   $x10 = COPY [[COPY]](s64)
  ; LP64-NEXT:   PseudoRET implicit $x10
  ;
  ; LP64F-LABEL: name: caller_double_ret
  ; LP64F: bb.1 (%ir-block.0):
  ; LP64F-NEXT:   ADJCALLSTACKDOWN 0, 0, implicit-def $x2, implicit $x2
  ; LP64F-NEXT:   PseudoCALL target-flags(riscv-call) @callee_double_ret, csr_ilp32f_lp64f, implicit-def $x1, implicit-def $x10
  ; LP64F-NEXT:   ADJCALLSTACKUP 0, 0, implicit-def $x2, implicit $x2
  ; LP64F-NEXT:   [[COPY:%[0-9]+]]:_(s64) = COPY $x10
  ; LP64F-NEXT:   $x10 = COPY [[COPY]](s64)
  ; LP64F-NEXT:   PseudoRET implicit $x10
  %1 = call double @callee_double_ret()
  %2 = bitcast double %1 to i64
  ret i64 %2
}