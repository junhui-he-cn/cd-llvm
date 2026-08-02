; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare i64 @llvm.cd.variant.field(ptr, i32)

define i32 @main() {
entry:
  %field = call i64 @llvm.cd.variant.field(ptr null, i32 0)
  ret i32 0
}

; DIRECT: variant_field
; MACHINE: variant_field
