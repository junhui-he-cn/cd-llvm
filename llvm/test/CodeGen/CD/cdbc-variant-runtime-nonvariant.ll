; RUN: not --crash llc -mtriple=cd-unknown-unknown %s -o - 2>&1 | FileCheck --check-prefix=DIRECT %s
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - 2>&1 | FileCheck --check-prefix=MACHINE %s

declare i64 @llvm.cd.variant.field(ptr, i32)

define i32 @main() {
entry:
  %field = call i64 @llvm.cd.variant.field(ptr null, i32 0)
  ret i32 0
}

; DIRECT: CD bytecode 0.2 lowering failed: llvm.cd.variant.field requires a statically identifiable variant for cdbc 0.2
; MACHINE: CD bytecode 0.2 lowering failed: llvm.cd.variant.field requires a statically identifiable variant for cdbc 0.2
