; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@contains_name = private unnamed_addr constant [9 x i8] c"contains\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.string(ptr)
declare i1 @llvm.cd.native(ptr, ...)
declare void @cd_print(i1)

define i32 @main() {
entry:
  %value = call ptr @llvm.cd.string(ptr @message)
  %result = call i1 (ptr, ...) @llvm.cd.native(
      ptr @contains_name, ptr %value, i64 1)
  call void @cd_print(i1 %result)
  ret i32 0
}

; DIRECT: call_native i0
; MACHINE: call_native i0
