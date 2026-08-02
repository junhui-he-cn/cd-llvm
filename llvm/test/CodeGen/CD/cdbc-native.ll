; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@floor_name = private unnamed_addr constant [6 x i8] c"floor\00"
@ceil_name = private unnamed_addr constant [5 x i8] c"ceil\00"
@sqrt_name = private unnamed_addr constant [5 x i8] c"sqrt\00"
@hash_name = private unnamed_addr constant [5 x i8] c"hash\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
declare void @print(double)

define i32 @main() {
entry:
  %floor = call double (ptr, ...) @llvm.cd.native(
      ptr @floor_name, double 1.75)
  %ceil = call double (ptr, ...) @llvm.cd.native(
      ptr @ceil_name, double 1.25)
  %sqrt = call double (ptr, ...) @llvm.cd.native(
      ptr @sqrt_name, double 9.0)
  %string = call ptr @llvm.cd.string(ptr @message)
  %hash = call double (ptr, ...) @llvm.cd.native(
      ptr @hash_name, ptr %string)
  call void @print(double %floor)
  call void @print(double %ceil)
  call void @print(double %sqrt)
  call void @print(double %hash)
  ret i32 0
}

; DIRECT: native_call
; DIRECT: native_call
; DIRECT: native_call
; DIRECT: native_call
; MACHINE: native_call
; MACHINE: native_call
; MACHINE: native_call
; MACHINE: native_call
