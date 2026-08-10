; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@clear_name = private unnamed_addr constant [6 x i8] c"clear\00"
@first = private unnamed_addr constant [2 x i8] c"a\00"
@second = private unnamed_addr constant [2 x i8] c"b\00"

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %first_key = call ptr @llvm.cd.string(ptr @first)
  %second_key = call ptr @llvm.cd.string(ptr @second)
  %map = call ptr (i32, ...) @llvm.cd.map(
      i32 2, ptr %first_key, i64 1, ptr %second_key, i64 2)
  %result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @clear_name, ptr %map)
  call void @cd_print(ptr %result)
  call void @cd_print(ptr %map)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT: native_call
; DIRECT: print
; DIRECT: print
; MACHINE: cdbc 0.1
; MACHINE: native_call
; MACHINE: print
; MACHINE: print
