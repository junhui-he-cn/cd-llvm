; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@point = private unnamed_addr constant [6 x i8] c"Point\00"
@field_x = private unnamed_addr constant [2 x i8] c"x\00"
@field_y = private unnamed_addr constant [2 x i8] c"y\00"

declare ptr @llvm.cd.struct(ptr, i32, ...)
declare i64 @llvm.cd.field(ptr, ptr)
declare i64 @llvm.cd.assign.field(ptr, ptr, i64)
declare void @cd_print(ptr)
declare void @print(i64)

define i32 @main() {
entry:
  %anonymous = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 0)
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr @point, i32 2, ptr @field_x, i64 10, ptr @field_y, i64 20)
  %x = call i64 @llvm.cd.field(ptr %object, ptr @field_x)
  %assigned = call i64 @llvm.cd.assign.field(
      ptr %object, ptr @field_x, i64 42)
  call void @cd_print(ptr %anonymous)
  call void @cd_print(ptr %object)
  call void @print(i64 %x)
  call void @print(i64 %assigned)
  ret i32 0
}

; DIRECT: struct {}
; DIRECT: struct n{{[0-9]+}}
; DIRECT: field
; DIRECT: assign_field
; MACHINE: struct {}
; MACHINE: struct n{{[0-9]+}}
; MACHINE: field
; MACHINE: assign_field
