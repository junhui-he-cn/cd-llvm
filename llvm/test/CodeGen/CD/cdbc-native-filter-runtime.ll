; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.machine

@filter_name = private unnamed_addr constant [7 x i8] c"filter\00"
@value = private unnamed_addr constant [6 x i8] c"value\00"

declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)

define i1 @keep(ptr %value) #0 {
entry:
  ret i1 true
}

define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @value)
  %filtered = call ptr (ptr, ...) @llvm.cd.native(
      ptr @filter_name, ptr %source, ptr @keep)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

; ARTIFACT: cdbc 0.2
; ARTIFACT: call_native i0
