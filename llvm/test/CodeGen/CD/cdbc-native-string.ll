; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@substr_name = private unnamed_addr constant [7 x i8] c"substr\00"
@char_at_name = private unnamed_addr constant [7 x i8] c"charAt\00"
@ascii = private unnamed_addr constant [6 x i8] c"hello\00"
@utf8 = private unnamed_addr constant [11 x i8] c"\E4\BD\A0\F0\9F\99\82e\CC\81\00"

declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %ascii_source = call ptr @llvm.cd.string(ptr @ascii)
  %empty = call ptr (ptr, ...) @llvm.cd.native(
      ptr @substr_name, ptr %ascii_source, double 5.0, double 0.0)
  %source = call ptr @llvm.cd.string(ptr @utf8)
  %part = call ptr (ptr, ...) @llvm.cd.native(
      ptr @substr_name, ptr %source, double 1.0, double 2.0)
  %character = call ptr (ptr, ...) @llvm.cd.native(
      ptr @char_at_name, ptr %source, double 1.0)
  call void @cd_print(ptr %empty)
  call void @cd_print(ptr %part)
  call void @cd_print(ptr %character)
  ret i32 0
}

; DIRECT: native_call
; DIRECT: native_call
; DIRECT: native_call
; MACHINE: native_call
; MACHINE: native_call
; MACHINE: native_call
