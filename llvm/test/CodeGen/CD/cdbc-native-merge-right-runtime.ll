; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@merge_name = private unnamed_addr constant [6 x i8] c"merge\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)

define i32 @main() {
entry:
  %left = call ptr (i32, ...) @llvm.cd.map(i32 0)
  %right = call ptr @llvm.cd.string(ptr @message)
  %result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @merge_name, ptr %left, ptr %right)
  ret i32 0
}

; DIRECT: call_native i0
; MACHINE: call_native i0
