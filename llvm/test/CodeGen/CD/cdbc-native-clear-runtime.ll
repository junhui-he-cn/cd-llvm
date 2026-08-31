; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@clear_name = private unnamed_addr constant [6 x i8] c"clear\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)

define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @message)
  %result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @clear_name, ptr %value)
  ret i32 0
}

; DIRECT: call_native i0
; MACHINE: call_native i0
