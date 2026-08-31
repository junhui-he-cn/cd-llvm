; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@copy_name = private unnamed_addr constant [5 x i8] c"copy\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %values = call ptr (i32, ...) @llvm.cd.array(
      i32 3, i64 1, i64 2, i64 3)
  %empty_copy = call ptr (ptr, ...) @llvm.cd.native(
      ptr @copy_name, ptr %empty)
  %values_copy = call ptr (ptr, ...) @llvm.cd.native(
      ptr @copy_name, ptr %values)
  call void @cd_print(ptr %empty_copy)
  call void @cd_print(ptr %values_copy)
  ret i32 0
}

; DIRECT: cdbc 0.2
; DIRECT-COUNT-2: call_native i0
; DIRECT-COUNT-2: call_native i1
; MACHINE: cdbc 0.2
; MACHINE-COUNT-2: call_native i0
; MACHINE-COUNT-2: call_native i1
