; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/any-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=ANY-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/any-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=ANY-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/any-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ANY-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/any-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ANY-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/any-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=ANY-CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/any-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=ANY-CALLBACK-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/all-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=ALL-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/all-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=ALL-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/all-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ALL-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/all-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ALL-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/all-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=ALL-CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/all-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=ALL-CALLBACK-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/count-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/count-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/count-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/count-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/count-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/count-callback.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-CALLBACK-MACHINE

; ANY-SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native any requires a CD dynamic-value array, a direct callback, and an i1 result
; ANY-SHAPE-MACHINE: CD machine backend does not support llvm.cd.native any requires a CD dynamic-value array, a direct callback, and an i1 result
; ANY-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native any requires a CD dynamic-value array, a direct callback, and an i1 result
; ANY-POINTER-MACHINE: CD machine backend does not support llvm.cd.native any requires a CD dynamic-value array, a direct callback, and an i1 result
; ANY-CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native any requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; ANY-CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native any requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; ALL-SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native all requires a CD dynamic-value array, a direct callback, and an i1 result
; ALL-SHAPE-MACHINE: CD machine backend does not support llvm.cd.native all requires a CD dynamic-value array, a direct callback, and an i1 result
; ALL-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native all requires a CD dynamic-value array, a direct callback, and an i1 result
; ALL-POINTER-MACHINE: CD machine backend does not support llvm.cd.native all requires a CD dynamic-value array, a direct callback, and an i1 result
; ALL-CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native all requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; ALL-CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native all requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; COUNT-SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native count requires a CD dynamic-value array, a direct callback, and a double result
; COUNT-SHAPE-MACHINE: CD machine backend does not support llvm.cd.native count requires a CD dynamic-value array, a direct callback, and a double result
; COUNT-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native count requires a CD dynamic-value array, a direct callback, and a double result
; COUNT-POINTER-MACHINE: CD machine backend does not support llvm.cd.native count requires a CD dynamic-value array, a direct callback, and a double result
; COUNT-CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native count requires a direct defined callback with one address-space-zero CD parameter and an i1 result
; COUNT-CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native count requires a direct defined callback with one address-space-zero CD parameter and an i1 result

;--- any-shape.ll
@name = private unnamed_addr constant [4 x i8] c"any\00"
declare i1 @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call i1 (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- any-pointer.ll
@name = private unnamed_addr constant [4 x i8] c"any\00"
declare i1 @llvm.cd.native(ptr, ...)
define i1 @yes(ptr %value) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %source = inttoptr i64 1 to ptr
  %value = call i1 (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @yes)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" }

;--- any-callback.ll
@name = private unnamed_addr constant [4 x i8] c"any\00"
declare i1 @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @returns_value(ptr %value) #0 {
entry:
  ret ptr %value
}
define i32 @main() {
entry:
  %source = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call i1 (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @returns_value)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- count-shape.ll
@name = private unnamed_addr constant [6 x i8] c"count\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- count-pointer.ll
@name = private unnamed_addr constant [6 x i8] c"count\00"
declare double @llvm.cd.native(ptr, ...)
define i1 @yes(ptr %value) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %source = inttoptr i64 1 to ptr
  %value = call double (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @yes)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" }

;--- count-callback.ll
@name = private unnamed_addr constant [6 x i8] c"count\00"
declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @returns_value(ptr %value) #0 {
entry:
  ret ptr %value
}
define i32 @main() {
entry:
  %source = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call double (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @returns_value)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- all-shape.ll
@name = private unnamed_addr constant [4 x i8] c"all\00"
declare i1 @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call i1 (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- all-pointer.ll
@name = private unnamed_addr constant [4 x i8] c"all\00"
declare i1 @llvm.cd.native(ptr, ...)
define i1 @yes(ptr %value) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %source = inttoptr i64 1 to ptr
  %value = call i1 (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @yes)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" }

;--- all-callback.ll
@name = private unnamed_addr constant [4 x i8] c"all\00"
declare i1 @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @returns_value(ptr %value) #0 {
entry:
  ret ptr %value
}
define i32 @main() {
entry:
  %source = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call i1 (ptr, ...) @llvm.cd.native(
      ptr @name, ptr %source, ptr @returns_value)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }
