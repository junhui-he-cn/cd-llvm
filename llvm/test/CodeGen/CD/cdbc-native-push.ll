; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@push_name = private unnamed_addr constant [5 x i8] c"push\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %message_value = call ptr @llvm.cd.string(ptr @message)
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 1)
  %first_result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @push_name, ptr %array, i64 2)
  %second_result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @push_name, ptr %array, ptr %message_value)
  call void @cd_print(ptr %first_result)
  call void @cd_print(ptr %second_result)
  call void @cd_print(ptr %array)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT: native_call
; DIRECT: native_call
; DIRECT-COUNT-3: print
; MACHINE: cdbc 0.1
; MACHINE: native_call
; MACHINE: native_call
; MACHINE-COUNT-3: print
