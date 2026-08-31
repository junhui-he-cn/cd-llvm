; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@pop_name = private unnamed_addr constant [4 x i8] c"pop\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %message_value = call ptr @llvm.cd.string(ptr @message)
  %array = call ptr (i32, ...) @llvm.cd.array(
      i32 2, i64 1, ptr %message_value)
  %first_result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @pop_name, ptr %array)
  %second_result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @pop_name, ptr %array)
  call void @cd_print(ptr %first_result)
  call void @cd_print(ptr %second_result)
  call void @cd_print(ptr %array)
  ret i32 0
}

; DIRECT: cdbc 0.2
; DIRECT: call_native i0
; DIRECT: call_native i0
; DIRECT-COUNT-3: call_native i1
; MACHINE: cdbc 0.2
; MACHINE: call_native i0
; MACHINE: call_native i0
; MACHINE-COUNT-3: call_native i1
