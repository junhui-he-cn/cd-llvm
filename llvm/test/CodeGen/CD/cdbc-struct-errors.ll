; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/count.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/count.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/type-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/type-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/field-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=FIELD-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/field-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=FIELD-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/value-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=VALUE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/value-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=VALUE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/object-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=OBJECT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/object-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=OBJECT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/name-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=NAME-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/name-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=NAME-MACHINE

; COUNT-DIRECT: CD target does not support LLVM operation: llvm.cd.struct field-count does not match the field name/value operand list
; COUNT-MACHINE: CD machine backend does not support llvm.cd.struct field-count does not match the field name/value operand list
; TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.struct requires an anonymous nil or private string global type name
; TYPE-MACHINE: CD machine backend does not support llvm.cd.struct requires an anonymous nil or private string global type name
; FIELD-DIRECT: CD target does not support LLVM operation: llvm.cd.struct requires a private non-empty string global field name
; FIELD-MACHINE: CD machine backend does not support llvm.cd.struct requires a private non-empty string global field name
; VALUE-DIRECT: CD target does not support LLVM operation: llvm.cd.struct requires scalar, nil, or CD dynamic-value field operands
; VALUE-MACHINE: CD machine backend does not support llvm.cd.struct requires scalar, nil, or CD dynamic-value field operands
; OBJECT-DIRECT: CD target does not support LLVM operation: llvm.cd.field requires a CD dynamic-value object
; OBJECT-MACHINE: CD machine backend does not support llvm.cd.field requires a CD dynamic-value object
; NAME-DIRECT: CD target does not support LLVM operation: llvm.cd.field requires a private non-empty string global field name
; NAME-MACHINE: CD machine backend does not support llvm.cd.field requires a private non-empty string global field name

;--- count.ll
declare ptr @llvm.cd.struct(ptr, i32, ...)

define i32 @main() {
entry:
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 2, ptr null, i64 1)
  ret i32 0
}

;--- type-pointer.ll
declare ptr @llvm.cd.struct(ptr, i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr %slot, i32 0)
  ret i32 0
}

;--- field-pointer.ll
declare ptr @llvm.cd.struct(ptr, i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 1, ptr %slot, i64 1)
  ret i32 0
}

;--- value-pointer.ll
@field_value = private unnamed_addr constant [6 x i8] c"value\00"

declare ptr @llvm.cd.struct(ptr, i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 1, ptr @field_value, ptr %slot)
  ret i32 0
}

;--- object-pointer.ll
declare i64 @llvm.cd.field(ptr, ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %value = call i64 @llvm.cd.field(ptr %slot, ptr null)
  ret i32 0
}

;--- name-pointer.ll
declare ptr @llvm.cd.field(ptr, ptr)

define i32 @main() {
entry:
  %object = call ptr @llvm.cd.field(ptr null, ptr null)
  ret i32 0
}
