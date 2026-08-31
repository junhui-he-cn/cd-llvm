; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=ARTIFACT %s < %t.machine

@count_name = private unnamed_addr constant [6 x i8] c"count\00"
@value = private unnamed_addr constant [6 x i8] c"value\00"

declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)

define i1 @yes(ptr %value) #0 {
entry:
  ret i1 true
}

define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @value)
  %result = call double (ptr, ...) @llvm.cd.native(
      ptr @count_name, ptr %source, ptr @yes)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

; ARTIFACT: cdbc 0.2
; ARTIFACT: call_native i0
