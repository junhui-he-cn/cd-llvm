; RUN: not --crash llc -mtriple=cd-unknown-unknown %s -o - 2>&1 | FileCheck --check-prefix=DIRECT %s
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - 2>&1 | FileCheck --check-prefix=MACHINE %s

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

; DIRECT: CD bytecode 0.2 lowering failed: variant field index is outside the inferred payload layout
; MACHINE: CD bytecode 0.2 lowering failed: variant field index is outside the inferred payload layout
