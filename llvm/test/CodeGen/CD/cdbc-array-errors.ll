; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/mismatched-count.ll -o - 2>&1 | FileCheck %s --check-prefix=MISMATCH-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/mismatched-count.ll -o - 2>&1 | FileCheck %s --check-prefix=MISMATCH-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/ordinary-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/ordinary-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/vector.ll -o - 2>&1 | FileCheck %s --check-prefix=VECTOR-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/vector.ll -o - 2>&1 | FileCheck %s --check-prefix=VECTOR-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/foreign-null.ll -o - 2>&1 | FileCheck %s --check-prefix=NULL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/foreign-null.ll -o - 2>&1 | FileCheck %s --check-prefix=NULL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/poison.ll -o - 2>&1 | FileCheck %s --check-prefix=POISON-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/poison.ll -o - 2>&1 | FileCheck %s --check-prefix=POISON-MACHINE
; RUN: not llc -mtriple=cd-unknown-unknown %t/nonconstant-count.ll -o - 2>&1 | FileCheck %s --check-prefix=NONCONST

; MISMATCH-DIRECT: CD target does not support LLVM operation: llvm.cd.array element-count does not match the operand list
; MISMATCH-MACHINE: CD machine backend does not support llvm.cd.array element-count does not match the operand list
; POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.array requires scalar, nil, string-token, or array-token operands
; POINTER-MACHINE: CD machine backend does not support llvm.cd.array requires scalar, nil, string-token, or array-token operands
; VECTOR-DIRECT: CD target does not support LLVM operation: llvm.cd.array requires scalar, nil, string-token, or array-token operands
; VECTOR-MACHINE: CD machine backend does not support llvm.cd.array requires scalar, nil, string-token, or array-token operands
; NULL-DIRECT: CD target does not support LLVM operation: llvm.cd.array requires scalar, nil, string-token, or array-token operands
; NULL-MACHINE: CD machine backend does not support llvm.cd.array requires scalar, nil, string-token, or array-token operands
; POISON-DIRECT: CD target does not support LLVM operation: llvm.cd.array requires scalar, nil, string-token, or array-token operands
; POISON-MACHINE: CD machine backend does not support llvm.cd.array requires scalar, nil, string-token, or array-token operands
; NONCONST: immarg operand has non-immediate parameter

;--- mismatched-count.ll
declare ptr @llvm.cd.array(i32, ...)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 1, i64 2)
  ret i32 0
}

;--- ordinary-pointer.ll
declare ptr @llvm.cd.array(i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr %slot)
  ret i32 0
}

;--- vector.ll
declare ptr @llvm.cd.array(i32, ...)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, <2 x i32> <i32 1, i32 2>)
  ret i32 0
}

;--- foreign-null.ll
declare ptr @llvm.cd.array(i32, ...)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, ptr addrspace(1) null)
  ret i32 0
}

;--- poison.ll
declare ptr @llvm.cd.array(i32, ...)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 poison)
  ret i32 0
}

;--- nonconstant-count.ll
declare ptr @llvm.cd.array(i32, ...)

define i32 @main() {
entry:
  %count = add i32 0, 1
  %array = call ptr (i32, ...) @llvm.cd.array(i32 %count, i64 1)
  ret i32 0
}
