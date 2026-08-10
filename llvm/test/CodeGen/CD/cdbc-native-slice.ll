; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@slice_name = private unnamed_addr constant [6 x i8] c"slice\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(
      i32 4, i64 1, i64 2, i64 3, i64 4)
  %middle = call ptr (ptr, ...) @llvm.cd.native(
      ptr @slice_name, ptr %array, double 1.0, double 2.0)
  %empty = call ptr (ptr, ...) @llvm.cd.native(
      ptr @slice_name, ptr %array, double 0.0, double 0.0)
  %tail = call ptr (ptr, ...) @llvm.cd.native(
      ptr @slice_name, ptr %array, double 2.0, double 2.0)
  call void @cd_print(ptr %middle)
  call void @cd_print(ptr %empty)
  call void @cd_print(ptr %tail)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT-COUNT-3: native_call
; DIRECT-COUNT-3: print
; MACHINE: cdbc 0.1
; MACHINE-COUNT-3: native_call
; MACHINE-COUNT-3: print
