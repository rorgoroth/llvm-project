; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py UTC_ARGS: --version 2
; RUN: llc -global-isel=0 -mtriple=amdgcn -mcpu=tahiti -amdgpu-codegenprepare-mul24=0 -amdgpu-codegenprepare-disable-idiv-expansion < %s | FileCheck -check-prefixes=SI,SI-SDAG %s
; RUN: llc -global-isel=1 -mtriple=amdgcn -mcpu=tahiti -amdgpu-codegenprepare-mul24=0 -amdgpu-codegenprepare-disable-idiv-expansion < %s | FileCheck -check-prefixes=SI,SI-GISEL %s

declare i32 @llvm.smin.i32(i32, i32)
declare i32 @llvm.smax.i32(i32, i32)
declare i32 @llvm.umin.i32(i32, i32)
declare i32 @llvm.umax.i32(i32, i32)

; Test computeKnownBits for umed3 node. We know the base address has a
; 0 sign bit only after umed3 is formed. The DS instruction offset can
; only be folded on SI with a positive base address.
define i32 @v_known_bits_umed3(i8 %a) {
; SI-SDAG-LABEL: v_known_bits_umed3:
; SI-SDAG:       ; %bb.0:
; SI-SDAG-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-SDAG-NEXT:    v_and_b32_e32 v0, 0xff, v0
; SI-SDAG-NEXT:    v_mov_b32_e32 v1, 0x80
; SI-SDAG-NEXT:    v_med3_u32 v0, v0, 32, v1
; SI-SDAG-NEXT:    s_mov_b32 m0, -1
; SI-SDAG-NEXT:    ds_read_u8 v0, v0 offset:128
; SI-SDAG-NEXT:    s_waitcnt lgkmcnt(0)
; SI-SDAG-NEXT:    s_setpc_b64 s[30:31]
;
; SI-GISEL-LABEL: v_known_bits_umed3:
; SI-GISEL:       ; %bb.0:
; SI-GISEL-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-GISEL-NEXT:    v_and_b32_e32 v0, 0xff, v0
; SI-GISEL-NEXT:    v_mov_b32_e32 v1, 0x80
; SI-GISEL-NEXT:    v_med3_u32 v0, v0, 32, v1
; SI-GISEL-NEXT:    ds_read_u8 v0, v0 offset:128
; SI-GISEL-NEXT:    s_waitcnt lgkmcnt(0)
; SI-GISEL-NEXT:    s_setpc_b64 s[30:31]
  %ext.a = zext i8 %a to i32
  %max.a = call i32 @llvm.umax.i32(i32 %ext.a, i32 32)
  %umed3 = call i32 @llvm.umin.i32(i32 %max.a, i32 128)
  %cast.umed3 = inttoptr i32 %umed3 to ptr addrspace(3)
  %gep = getelementptr i8, ptr addrspace(3) %cast.umed3, i32 128
  %load = load i8, ptr addrspace(3) %gep
  %result = zext i8 %load to i32
  ret i32 %result
}

; The IR expansion of division is disabled. The division is legalized
; late, after the formation of smed3. We need to be able to
; computeNumSignBits on the smed3 in order to use the 24-bit-as-float
; sdiv legalization.
define i32 @v_known_signbits_smed3(i16 %a, i16 %b) {
; SI-SDAG-LABEL: v_known_signbits_smed3:
; SI-SDAG:       ; %bb.0:
; SI-SDAG-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-SDAG-NEXT:    v_bfe_i32 v1, v1, 0, 16
; SI-SDAG-NEXT:    s_movk_i32 s4, 0xffc0
; SI-SDAG-NEXT:    v_mov_b32_e32 v2, 0x80
; SI-SDAG-NEXT:    v_med3_i32 v1, v1, s4, v2
; SI-SDAG-NEXT:    v_cvt_f32_i32_e32 v2, v1
; SI-SDAG-NEXT:    v_bfe_i32 v0, v0, 0, 16
; SI-SDAG-NEXT:    s_movk_i32 s4, 0xffe0
; SI-SDAG-NEXT:    v_med3_i32 v0, v0, s4, 64
; SI-SDAG-NEXT:    v_cvt_f32_i32_e32 v3, v0
; SI-SDAG-NEXT:    v_rcp_iflag_f32_e32 v4, v2
; SI-SDAG-NEXT:    v_xor_b32_e32 v0, v0, v1
; SI-SDAG-NEXT:    v_ashrrev_i32_e32 v0, 30, v0
; SI-SDAG-NEXT:    v_or_b32_e32 v0, 1, v0
; SI-SDAG-NEXT:    v_mul_f32_e32 v1, v3, v4
; SI-SDAG-NEXT:    v_trunc_f32_e32 v1, v1
; SI-SDAG-NEXT:    v_mad_f32 v3, -v1, v2, v3
; SI-SDAG-NEXT:    v_cvt_i32_f32_e32 v1, v1
; SI-SDAG-NEXT:    v_cmp_ge_f32_e64 vcc, |v3|, |v2|
; SI-SDAG-NEXT:    v_cndmask_b32_e32 v0, 0, v0, vcc
; SI-SDAG-NEXT:    v_add_i32_e32 v0, vcc, v1, v0
; SI-SDAG-NEXT:    v_bfe_i32 v0, v0, 0, 16
; SI-SDAG-NEXT:    s_setpc_b64 s[30:31]
;
; SI-GISEL-LABEL: v_known_signbits_smed3:
; SI-GISEL:       ; %bb.0:
; SI-GISEL-NEXT:    s_waitcnt vmcnt(0) expcnt(0) lgkmcnt(0)
; SI-GISEL-NEXT:    v_bfe_i32 v1, v1, 0, 16
; SI-GISEL-NEXT:    v_mov_b32_e32 v2, 0xffffffc0
; SI-GISEL-NEXT:    v_mov_b32_e32 v3, 0x80
; SI-GISEL-NEXT:    v_med3_i32 v1, v1, v2, v3
; SI-GISEL-NEXT:    v_ashrrev_i32_e32 v2, 31, v1
; SI-GISEL-NEXT:    v_add_i32_e32 v1, vcc, v1, v2
; SI-GISEL-NEXT:    v_xor_b32_e32 v1, v1, v2
; SI-GISEL-NEXT:    v_cvt_f32_u32_e32 v3, v1
; SI-GISEL-NEXT:    v_sub_i32_e32 v5, vcc, 0, v1
; SI-GISEL-NEXT:    v_mov_b32_e32 v4, 0xffffffe0
; SI-GISEL-NEXT:    v_rcp_iflag_f32_e32 v3, v3
; SI-GISEL-NEXT:    v_bfe_i32 v0, v0, 0, 16
; SI-GISEL-NEXT:    v_med3_i32 v0, v0, v4, 64
; SI-GISEL-NEXT:    v_ashrrev_i32_e32 v4, 31, v0
; SI-GISEL-NEXT:    v_mul_f32_e32 v3, 0x4f7ffffe, v3
; SI-GISEL-NEXT:    v_cvt_u32_f32_e32 v3, v3
; SI-GISEL-NEXT:    v_add_i32_e32 v0, vcc, v0, v4
; SI-GISEL-NEXT:    v_xor_b32_e32 v0, v0, v4
; SI-GISEL-NEXT:    v_mul_lo_u32 v5, v5, v3
; SI-GISEL-NEXT:    v_mul_hi_u32 v5, v3, v5
; SI-GISEL-NEXT:    v_add_i32_e32 v3, vcc, v3, v5
; SI-GISEL-NEXT:    v_mul_hi_u32 v3, v0, v3
; SI-GISEL-NEXT:    v_mul_lo_u32 v5, v3, v1
; SI-GISEL-NEXT:    v_add_i32_e32 v6, vcc, 1, v3
; SI-GISEL-NEXT:    v_sub_i32_e32 v0, vcc, v0, v5
; SI-GISEL-NEXT:    v_cmp_ge_u32_e32 vcc, v0, v1
; SI-GISEL-NEXT:    v_cndmask_b32_e32 v3, v3, v6, vcc
; SI-GISEL-NEXT:    v_sub_i32_e64 v5, s[4:5], v0, v1
; SI-GISEL-NEXT:    v_cndmask_b32_e32 v0, v0, v5, vcc
; SI-GISEL-NEXT:    v_add_i32_e32 v5, vcc, 1, v3
; SI-GISEL-NEXT:    v_cmp_ge_u32_e32 vcc, v0, v1
; SI-GISEL-NEXT:    v_cndmask_b32_e32 v0, v3, v5, vcc
; SI-GISEL-NEXT:    v_xor_b32_e32 v1, v4, v2
; SI-GISEL-NEXT:    v_xor_b32_e32 v0, v0, v1
; SI-GISEL-NEXT:    v_sub_i32_e32 v0, vcc, v0, v1
; SI-GISEL-NEXT:    s_setpc_b64 s[30:31]
  %ext.a = sext i16 %a to i32
  %max.a = call i32 @llvm.smax.i32(i32 %ext.a, i32 -32)
  %smed3.a = call i32 @llvm.smin.i32(i32 %max.a, i32 64)
  %ext.b = sext i16 %b to i32
  %max.b = call i32 @llvm.smax.i32(i32 %ext.b, i32 -64)
  %smed3.b = call i32 @llvm.smin.i32(i32 %max.b, i32 128)
  %mul = sdiv i32 %smed3.a, %smed3.b
  ret i32 %mul
}
;; NOTE: These prefixes are unused and the list is autogenerated. Do not add tests below this line:
; SI: {{.*}}