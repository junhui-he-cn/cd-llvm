; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.index(ptr, double)
declare double @llvm.cd.len(ptr)
declare void @cd_print(ptr)
declare void @print(double)

define i32 @main() {
entry:
  %inner = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 6, i64 7)
  %outer = call ptr (i32, ...) @llvm.cd.array(i32 2, ptr %inner, i64 11)
  %nested = call ptr @llvm.cd.index(ptr %outer, double 0.0)
  %nested_length = call double @llvm.cd.len(ptr %nested)
  %scalar = call ptr @llvm.cd.index(ptr %outer, double 1.0)
  %length = call double @llvm.cd.len(ptr %outer)
  call void @cd_print(ptr %nested)
  call void @cd_print(ptr %scalar)
  call void @print(double %nested_length)
  call void @print(double %length)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = index
; DIRECT: r{{[0-9]+}} = len
; DIRECT: r{{[0-9]+}} = index
; DIRECT: r{{[0-9]+}} = len
; MACHINE: r{{[0-9]+}} = index
; MACHINE: r{{[0-9]+}} = len
; MACHINE: r{{[0-9]+}} = index
; MACHINE: r{{[0-9]+}} = len
