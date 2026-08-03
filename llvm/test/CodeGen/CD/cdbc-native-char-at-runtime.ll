; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@char_at_name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @char_at_name, ptr %source, double 5.0)
  call void @cd_print(ptr %value)
  ret i32 0
}

; DIRECT: native_call
; MACHINE: native_call
