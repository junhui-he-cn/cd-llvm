; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@str_name = private unnamed_addr constant [4 x i8] c"str\00"
@type_name = private unnamed_addr constant [7 x i8] c"typeOf\00"
@range_name = private unnamed_addr constant [6 x i8] c"range\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.index(ptr, double)
declare double @llvm.cd.len(ptr)
declare void @cd_print(ptr)
declare void @print(double)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @message)
  %text = call ptr (ptr, ...) @llvm.cd.native(
      ptr @str_name, i64 7)
  %kind = call ptr (ptr, ...) @llvm.cd.native(
      ptr @type_name, ptr %string)
  %range = call ptr (ptr, ...) @llvm.cd.native(
      ptr @range_name, double 1.0, double 4.0)
  %item = call ptr @llvm.cd.index(ptr %range, double 1.0)
  %length = call double @llvm.cd.len(ptr %range)
  call void @cd_print(ptr %text)
  call void @cd_print(ptr %kind)
  call void @cd_print(ptr %item)
  call void @print(double %length)
  ret i32 0
}

; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: call_native i2
; DIRECT: index
; DIRECT: len
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: call_native i2
; MACHINE: index
; MACHINE: len
