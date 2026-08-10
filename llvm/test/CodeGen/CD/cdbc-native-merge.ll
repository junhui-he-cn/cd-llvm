; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@merge_name = private unnamed_addr constant [6 x i8] c"merge\00"
@first = private unnamed_addr constant [2 x i8] c"a\00"
@second = private unnamed_addr constant [2 x i8] c"b\00"
@third = private unnamed_addr constant [2 x i8] c"c\00"

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %first_key = call ptr @llvm.cd.string(ptr @first)
  %second_key = call ptr @llvm.cd.string(ptr @second)
  %third_key = call ptr @llvm.cd.string(ptr @third)
  %left = call ptr (i32, ...) @llvm.cd.map(
      i32 2, ptr %first_key, i64 1, ptr %second_key, i64 2)
  %right = call ptr (i32, ...) @llvm.cd.map(
      i32 2, ptr %second_key, i64 3, ptr %third_key, i64 4)
  %merged = call ptr (ptr, ...) @llvm.cd.native(
      ptr @merge_name, ptr %left, ptr %right)
  %empty = call ptr (i32, ...) @llvm.cd.map(i32 0)
  %merged_empty = call ptr (ptr, ...) @llvm.cd.native(
      ptr @merge_name, ptr %empty, ptr %right)
  call void @cd_print(ptr %merged)
  call void @cd_print(ptr %left)
  call void @cd_print(ptr %right)
  call void @cd_print(ptr %merged_empty)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT: native_call
; DIRECT: native_call
; DIRECT-COUNT-4: print
; MACHINE: cdbc 0.1
; MACHINE: native_call
; MACHINE: native_call
; MACHINE-COUNT-4: print
