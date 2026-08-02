; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/mismatched-count.ll -o - 2>&1 | FileCheck %s --check-prefix=MISMATCH-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/mismatched-count.ll -o - 2>&1 | FileCheck %s --check-prefix=MISMATCH-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/payload-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=PAYLOAD-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/payload-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=PAYLOAD-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/tag-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=TAG-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/tag-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=TAG-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/field-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=FIELD-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/field-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=FIELD-MACHINE
; RUN: not llc -mtriple=cd-unknown-unknown %t/nonconstant-count.ll -o - 2>&1 | FileCheck %s --check-prefix=NONCONST

; MISMATCH-DIRECT: CD target does not support LLVM operation: llvm.cd.variant field-count does not match the payload operand list
; MISMATCH-MACHINE: CD machine backend does not support llvm.cd.variant field-count does not match the payload operand list
; PAYLOAD-DIRECT: CD target does not support LLVM operation: llvm.cd.variant requires scalar, nil, or CD dynamic-value payload operands
; PAYLOAD-MACHINE: CD machine backend does not support llvm.cd.variant requires scalar, nil, or CD dynamic-value payload operands
; TAG-DIRECT: CD target does not support LLVM operation: llvm.cd.variant.tag requires a CD dynamic-value value
; TAG-MACHINE: CD machine backend does not support llvm.cd.variant.tag requires a CD dynamic-value value
; FIELD-DIRECT: CD target does not support LLVM operation: llvm.cd.variant.field requires a CD dynamic-value value
; FIELD-MACHINE: CD machine backend does not support llvm.cd.variant.field requires a CD dynamic-value value
; NONCONST: immarg operand has non-immediate parameter

;--- mismatched-count.ll
@enum = private unnamed_addr constant [5 x i8] c"Enum\00"
@variant = private unnamed_addr constant [2 x i8] c"A\00"

declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)

define i32 @main() {
entry:
  %value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
      ptr @enum, ptr @variant, i32 1)
  ret i32 0
}

;--- payload-pointer.ll
@enum = private unnamed_addr constant [5 x i8] c"Enum\00"
@variant = private unnamed_addr constant [2 x i8] c"A\00"

declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
      ptr @enum, ptr @variant, i32 1, ptr %slot)
  ret i32 0
}

;--- tag-pointer.ll
@enum = private unnamed_addr constant [5 x i8] c"Enum\00"
@variant = private unnamed_addr constant [2 x i8] c"A\00"

declare i1 @llvm.cd.variant.tag(ptr, ptr, ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %matches = call i1 @llvm.cd.variant.tag(
      ptr %slot, ptr @enum, ptr @variant)
  ret i32 0
}

;--- field-pointer.ll
declare i64 @llvm.cd.variant.field(ptr, i32)

define i32 @main() {
entry:
  %slot = alloca i8
  %value = call i64 @llvm.cd.variant.field(ptr %slot, i32 0)
  ret i32 0
}

;--- nonconstant-count.ll
@enum = private unnamed_addr constant [5 x i8] c"Enum\00"
@variant = private unnamed_addr constant [2 x i8] c"A\00"

declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)

define i32 @main() {
entry:
  %count = add i32 0, 1
  %value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
      ptr @enum, ptr @variant, i32 %count)
  ret i32 0
}
