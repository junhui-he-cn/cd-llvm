; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@remove_name = private unnamed_addr constant [7 x i8] c"remove\00"
@first = private unnamed_addr constant [2 x i8] c"a\00"

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %first_key = call ptr @llvm.cd.string(ptr @first)
  %map = call ptr (i32, ...) @llvm.cd.map(
      i32 2, ptr %first_key, i64 1, i64 2, i64 2)
  %removed_first = call ptr (ptr, ...) @llvm.cd.native(
      ptr @remove_name, ptr %map, ptr %first_key)
  call void @cd_print(ptr %removed_first)
  call void @cd_print(ptr %map)
  %removed_second = call ptr (ptr, ...) @llvm.cd.native(
      ptr @remove_name, ptr %map, i64 2)
  call void @cd_print(ptr %removed_second)
  call void @cd_print(ptr %map)
  ret i32 0
}

; DIRECT: cdbc 0.2
; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: call_native i1
; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: call_native i1
; MACHINE: cdbc 0.2
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: call_native i1
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: call_native i1
