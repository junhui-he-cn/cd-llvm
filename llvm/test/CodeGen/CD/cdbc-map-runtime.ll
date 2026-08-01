; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.index(ptr, double)

define i32 @main() {
entry:
  %map = call ptr (i32, ...) @llvm.cd.map(i32 1, i64 1, i64 10)
  %missing = call ptr @llvm.cd.index(ptr %map, double 2.0)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = map
; DIRECT: r{{[0-9]+}} = index
; MACHINE: r{{[0-9]+}} = map
; MACHINE: r{{[0-9]+}} = index
