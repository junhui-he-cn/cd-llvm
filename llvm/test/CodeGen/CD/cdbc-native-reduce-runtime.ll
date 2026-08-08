; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.machine

@reduce_name = private unnamed_addr constant [7 x i8] c"reduce\00"
@value = private unnamed_addr constant [6 x i8] c"value\00"

declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)

define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}

define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @value)
  %reduced = call ptr (ptr, ...) @llvm.cd.native(
      ptr @reduce_name, ptr %source, double 0.0, ptr @last)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

; ARTIFACT: cdbc 0.1
; ARTIFACT: native_call
