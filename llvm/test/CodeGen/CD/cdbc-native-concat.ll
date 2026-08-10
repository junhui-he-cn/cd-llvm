; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@concat_name = private unnamed_addr constant [7 x i8] c"concat\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %left = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 1, i64 2)
  %right = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 3, i64 4)
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %combined = call ptr (ptr, ...) @llvm.cd.native(
      ptr @concat_name, ptr %left, ptr %right)
  %left_only = call ptr (ptr, ...) @llvm.cd.native(
      ptr @concat_name, ptr %left, ptr %empty)
  call void @cd_print(ptr %combined)
  call void @cd_print(ptr %left_only)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT-COUNT-2: native_call
; DIRECT-COUNT-2: print
; MACHINE: cdbc 0.1
; MACHINE-COUNT-2: native_call
; MACHINE-COUNT-2: print
