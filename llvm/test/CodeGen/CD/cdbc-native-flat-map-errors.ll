; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-MACHINE

; SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native flatMap requires a CD dynamic-value array, a direct callback, and a ptr result
; SHAPE-MACHINE: CD machine backend does not support llvm.cd.native flatMap requires a CD dynamic-value array, a direct callback, and a ptr result
; POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native flatMap requires a CD dynamic-value array, a direct callback, and a ptr result
; POINTER-MACHINE: CD machine backend does not support llvm.cd.native flatMap requires a CD dynamic-value array, a direct callback, and a ptr result
; CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native flatMap requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result
; CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native flatMap requires a direct defined callback with one address-space-zero CD parameter and a cd.value.return pointer result

;--- shape.ll
@name = private unnamed_addr constant [8 x i8] c"flatMap\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- pointer.ll
@name = private unnamed_addr constant [8 x i8] c"flatMap\00"
declare ptr @llvm.cd.native(ptr, ...)
define ptr @callback(ptr %value) #0 {
entry:
  ret ptr %value
}
define i32 @main() {
entry:
  %source = inttoptr i64 1 to ptr
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @callback)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- callback.ll
@name = private unnamed_addr constant [8 x i8] c"flatMap\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define i64 @callback(i64 %value) {
entry:
  ret i64 %value
}
define i32 @main() {
entry:
  %source = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @callback)
  ret i32 0
}
