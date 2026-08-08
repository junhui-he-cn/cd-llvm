; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.machine

@flat_map_name = private unnamed_addr constant [8 x i8] c"flatMap\00"
@value = private unnamed_addr constant [6 x i8] c"value\00"

declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)

define ptr @identity(ptr %value) #0 {
entry:
  ret ptr %value
}

define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @value)
  %flattened = call ptr (ptr, ...) @llvm.cd.native(
      ptr @flat_map_name, ptr %source, ptr @identity)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" "cd.value.return" }

; ARTIFACT: cdbc 0.1
; ARTIFACT: native_call
