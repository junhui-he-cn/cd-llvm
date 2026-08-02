; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@option = private unnamed_addr constant [7 x i8] c"Option\00"
@none = private unnamed_addr constant [5 x i8] c"None\00"

declare ptr @llvm.cd.variant(ptr, ptr, i32, ...)
declare i64 @llvm.cd.variant.field(ptr, i32)

define i32 @main() {
entry:
  %value = call ptr (ptr, ptr, i32, ...) @llvm.cd.variant(
      ptr @option, ptr @none, i32 0)
  %field = call i64 @llvm.cd.variant.field(ptr %value, i32 0)
  ret i32 0
}

; DIRECT: variant
; DIRECT: variant_field
; MACHINE: variant
; MACHINE: variant_field
