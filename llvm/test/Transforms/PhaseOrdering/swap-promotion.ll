; NOTE: Assertions have been autogenerated by utils/update_test_checks.py
; RUN: opt -S -O1 < %s | FileCheck %s
; RUN: opt -S -O2 < %s | FileCheck %s
; RUN: opt -S -O3 < %s | FileCheck %s

define void @swap(ptr %p1, ptr %p2) {
; CHECK-LABEL: @swap(
; CHECK-NEXT:    [[TMP1:%.*]] = load i64, ptr [[P1:%.*]], align 1
; CHECK-NEXT:    [[TMP2:%.*]] = load i64, ptr [[P2:%.*]], align 1
; CHECK-NEXT:    store i64 [[TMP2]], ptr [[P1]], align 1
; CHECK-NEXT:    store i64 [[TMP1]], ptr [[P2]], align 1
; CHECK-NEXT:    ret void
;
  %tmp = alloca [2 x i32]
  call void @llvm.memcpy.p0.p0.i64(ptr %tmp, ptr %p1, i64 8, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr %p1, ptr %p2, i64 8, i1 false)
  call void @llvm.memcpy.p0.p0.i64(ptr %p2, ptr %tmp, i64 8, i1 false)
  ret void
}

define i32 @test(i32 %n) {
; CHECK-LABEL: @test(
; CHECK-NEXT:    br label [[LOOP:%.*]]
; CHECK:       loop:
; CHECK-NEXT:    [[P1_SROA_5_0:%.*]] = phi i32 [ 1, [[TMP0:%.*]] ], [ [[V2_NEXT:%.*]], [[LOOP]] ]
; CHECK-NEXT:    [[P1_SROA_0_0:%.*]] = phi i32 [ 0, [[TMP0]] ], [ [[V1_INC:%.*]], [[LOOP]] ]
; CHECK-NEXT:    [[V1_INC]] = add i32 [[P1_SROA_0_0]], 1
; CHECK-NEXT:    [[V2_NEXT]] = shl i32 [[P1_SROA_5_0]], 1
; CHECK-NEXT:    [[C:%.*]] = icmp eq i32 [[V1_INC]], [[N:%.*]]
; CHECK-NEXT:    br i1 [[C]], label [[EXIT:%.*]], label [[LOOP]]
; CHECK:       exit:
; CHECK-NEXT:    ret i32 [[V2_NEXT]]
;
  %p1 = alloca [2 x i32]
  %p2 = alloca [2 x i32]
  %p1.2 = getelementptr i32, ptr %p1, i64 1
  %p2.2 = getelementptr i32, ptr %p2, i64 1
  store i32 0, ptr %p1
  store i32 1, ptr %p1.2
  br label %loop

loop:
  %v1 = load i32, ptr %p1
  %v1.inc = add i32 %v1, 1
  store i32 %v1.inc, ptr %p1
  %v2 = load i32, ptr %p1.2
  %v2.next = shl i32 %v2, 1
  store i32 %v2.next, ptr %p1.2
  %c = icmp eq i32 %v1.inc, %n
  br i1 %c, label %exit, label %loop

exit:
  call void @swap(ptr %p1, ptr %p2)
  %res = load i32, ptr %p2.2
  ret i32 %res
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)