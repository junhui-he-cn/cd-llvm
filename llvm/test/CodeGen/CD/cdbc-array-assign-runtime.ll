; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.array(i32, ...)
declare double @llvm.cd.assign.index(ptr, double, double)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 1)
  %value = call double @llvm.cd.assign.index(
      ptr %array, double 2.0, double 9.0)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = assign_index
; MACHINE: r{{[0-9]+}} = assign_index
