; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@sqrt_name = private unnamed_addr constant [5 x i8] c"sqrt\00"

declare double @llvm.cd.native(ptr, ...)
declare void @print(double)

define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(
      ptr @sqrt_name, double -1.0)
  call void @print(double %value)
  ret i32 0
}

; DIRECT: call_native i0
; MACHINE: call_native i0
