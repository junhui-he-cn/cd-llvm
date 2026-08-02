; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@field_items = private unnamed_addr constant [6 x i8] c"items\00"

declare ptr @llvm.cd.struct(ptr, i32, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.field(ptr, ptr)
declare ptr @llvm.cd.assign.field(ptr, ptr, ptr)
declare double @llvm.cd.len(ptr)
declare void @cd_print(ptr)
declare void @print(double)

define i32 @main() {
entry:
  %values = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 1, i64 2)
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 1, ptr @field_items, ptr %values)
  %items = call ptr @llvm.cd.field(ptr %object, ptr @field_items)
  %replacement = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 9)
  %assigned = call ptr @llvm.cd.assign.field(
      ptr %object, ptr @field_items, ptr %replacement)
  %length = call double @llvm.cd.len(ptr %assigned)
  call void @cd_print(ptr %object)
  call void @cd_print(ptr %items)
  call void @print(double %length)
  ret i32 0
}

; DIRECT: struct {
; DIRECT: field
; DIRECT: assign_field
; DIRECT: len
; MACHINE: struct {
; MACHINE: field
; MACHINE: assign_field
; MACHINE: len
