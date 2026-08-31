; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@pop_name = private unnamed_addr constant [4 x i8] c"pop\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @pop_name, ptr %array)
  ret i32 0
}

; DIRECT: call_native i0
; MACHINE: call_native i0
