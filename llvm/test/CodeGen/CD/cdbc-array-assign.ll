; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.index(ptr, double)
declare ptr @llvm.cd.assign.index(ptr, double, ptr)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 1, i64 2)
  %replacement = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 7)
  %assigned_array = call ptr @llvm.cd.assign.index(
      ptr %array, double 0.0, ptr %replacement)
  %assigned_nil = call ptr @llvm.cd.assign.index(
      ptr %array, double 1.0, ptr null)
  %read_number = call ptr @llvm.cd.index(ptr %array, double 0.0)
  %read_array = call ptr @llvm.cd.index(ptr %array, double 1.0)
  call void @cd_print(ptr %read_number)
  call void @cd_print(ptr %assigned_array)
  call void @cd_print(ptr %assigned_nil)
  call void @cd_print(ptr %read_array)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = assign_index
; DIRECT: r{{[0-9]+}} = assign_index
; MACHINE: r{{[0-9]+}} = assign_index
; MACHINE: r{{[0-9]+}} = assign_index
