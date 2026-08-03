; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@field_value = private unnamed_addr constant [6 x i8] c"value\00"

declare ptr @llvm.cd.struct(ptr, i32, ...)
declare i64 @llvm.cd.assign.field(ptr, ptr, i64)
declare ptr @llvm.cd.field(ptr, ptr)
declare void @cd_print(ptr)

define ptr @identity(i1 %choose, ptr %value) #0 {
entry:
  br i1 %choose, label %value_block, label %nil_block

value_block:
  ret ptr %value

nil_block:
  ret ptr null
}

define void @touch(ptr %object, i64 %replacement) #1 {
entry:
  %assigned = call i64 @llvm.cd.assign.field(
      ptr %object, ptr @field_value, i64 %replacement)
  ret void
}

define i32 @main() {
entry:
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 1, ptr @field_value, i64 1)
  %roundtrip = call ptr @identity(i1 true, ptr %object)
  call void @touch(ptr %roundtrip, i64 7)
  %observed = call ptr @llvm.cd.field(ptr %roundtrip, ptr @field_value)
  call void @cd_print(ptr %observed)
  %nil = call ptr @identity(i1 false, ptr null)
  call void @cd_print(ptr %nil)
  ret i32 0
}

attributes #0 = { "cd.value.params"="1" "cd.value.return" }
attributes #1 = { "cd.value.params"="0" }

; DIRECT: cdbc 0.1
; DIRECT: main registers=
; DIRECT: make_function f0
; DIRECT: call r{{[0-9]+}} [r{{[0-9]+}}, r{{[0-9]+}}]
; DIRECT: make_function f1
; DIRECT: call r{{[0-9]+}} [r{{[0-9]+}}, r{{[0-9]+}}]
; DIRECT: function f0 name="identity" arity=2
; DIRECT: param 0 = "choose"
; DIRECT: param 1 = "value"
; DIRECT: return r{{[0-9]+}}
; DIRECT: function f1 name="touch" arity=2
; DIRECT: param 0 = "object"
; DIRECT: param 1 = "replacement"
; DIRECT: assign_field
; MACHINE: cdbc 0.1
; MACHINE: make_function f0
; MACHINE: call r{{[0-9]+}} [r{{[0-9]+}}, r{{[0-9]+}}]
; MACHINE: make_function f1
; MACHINE: call r{{[0-9]+}} [r{{[0-9]+}}, r{{[0-9]+}}]
; MACHINE: function f0 name="identity" arity=2
; MACHINE: param 0 = "choose"
; MACHINE: param 1 = "value"
; MACHINE: return r{{[0-9]+}}
; MACHINE: function f1 name="touch" arity=2
; MACHINE: param 0 = "object"
; MACHINE: param 1 = "replacement"
; MACHINE: assign_field
