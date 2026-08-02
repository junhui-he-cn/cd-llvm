; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@option = private unnamed_addr constant [7 x i8] c"Option\00"
@some = private unnamed_addr constant [5 x i8] c"Some\00"
@none = private unnamed_addr constant [5 x i8] c"None\00"

declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)
declare i1 @llvm.cd.variant.tag(ptr, ptr, ptr)
declare i64 @llvm.cd.variant.field(ptr, i32)
declare void @cd_print(ptr)
declare void @print(i64)

define i32 @main() {
entry:
  %value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
      ptr @option, ptr @some, i32 2, i64 7, double 1.5)
  %matches = call i1 @llvm.cd.variant.tag(
      ptr %value, ptr @option, ptr @some)
  %does_not_match = call i1 @llvm.cd.variant.tag(
      ptr %value, ptr @option, ptr @none)
  %integer = call i64 @llvm.cd.variant.field(ptr %value, i32 0)
  call void @cd_print(ptr %value)
  call void @print(i64 %integer)
  ret i32 0
}

; DIRECT: variant n{{[0-9]+}}.n{{[0-9]+}}
; DIRECT: variant_tag
; DIRECT: variant_tag
; DIRECT: variant_field
; MACHINE: variant n{{[0-9]+}}.n{{[0-9]+}}
; MACHINE: variant_tag
; MACHINE: variant_tag
; MACHINE: variant_field
