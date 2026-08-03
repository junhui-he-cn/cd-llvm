; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@substr_name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @substr_name, ptr %source, double 4.0, double 2.0)
  call void @cd_print(ptr %value)
  ret i32 0
}

; DIRECT: native_call
; MACHINE: native_call
