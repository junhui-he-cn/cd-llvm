; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@field_present = private unnamed_addr constant [8 x i8] c"present\00"
@field_missing = private unnamed_addr constant [8 x i8] c"missing\00"

declare ptr @llvm.cd.struct(ptr, i32, ...)
declare i64 @llvm.cd.field(ptr, ptr)

define i32 @main() {
entry:
  %object = call ptr (ptr, i32, ...) @llvm.cd.struct(
      ptr null, i32 1, ptr @field_present, i64 1)
  %missing = call i64 @llvm.cd.field(ptr %object, ptr @field_missing)
  ret i32 0
}

; DIRECT: types:
; DIRECT: t0 = struct "__cd_anonymous_struct_7_present" field0="present"
; DIRECT: make_struct t0
; DIRECT: field
; MACHINE: types:
; MACHINE: t0 = struct "__cd_anonymous_struct_7_present" field0="present"
; MACHINE: make_struct t0
; MACHINE: field
