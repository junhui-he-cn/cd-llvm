; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/initial-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=INITIAL-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/initial-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=INITIAL-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback-markers.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-MARKERS-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback-markers.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-MARKERS-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback-declaration.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-DECL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback-declaration.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-DECL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback-cast.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-CAST-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback-cast.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-CAST-MACHINE

; SHAPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a CD dynamic-value array, a scalar or CD dynamic-value initial value, a direct callback, and a ptr result
; SHAPE-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a CD dynamic-value array, a scalar or CD dynamic-value initial value, a direct callback, and a ptr result
; POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a CD dynamic-value array, a scalar or CD dynamic-value initial value, a direct callback, and a ptr result
; POINTER-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a CD dynamic-value array, a scalar or CD dynamic-value initial value, a direct callback, and a ptr result
; INITIAL-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a CD dynamic-value array, a scalar or CD dynamic-value initial value, a direct callback, and a ptr result
; INITIAL-POINTER-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a CD dynamic-value array, a scalar or CD dynamic-value initial value, a direct callback, and a ptr result
; CALLBACK-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-ARITY-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-MARKERS-DIRECT: CD target does not support LLVM operation: every pointer function parameter must be listed by cd.value.params
; CALLBACK-MARKERS-MACHINE: CD machine backend does not support every pointer function parameter must be listed by cd.value.params
; CALLBACK-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-RESULT-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-DECL-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-DECL-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-CAST-DIRECT: CD target does not support LLVM operation: llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result
; CALLBACK-CAST-MACHINE: CD machine backend does not support llvm.cd.native reduce requires a direct defined callback with two address-space-zero CD parameters and a cd.value.return pointer result

;--- shape.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, ptr @last)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

;--- pointer.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}
define i32 @main() {
entry:
  %array = inttoptr i64 1 to ptr
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, double 0.0, ptr @last)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

;--- initial-pointer.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %initial = inttoptr i64 1 to ptr
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, ptr %initial, ptr @last)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

;--- callback-arity.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @one(ptr %item) #0 {
entry:
  ret ptr %item
}
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, double 0.0, ptr @one)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- callback-markers.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, double 0.0, ptr @last)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0" "cd.value.return" }

;--- callback-result.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define i1 @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret i1 true
}
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, double 0.0, ptr @last)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0,1" }

;--- callback-declaration.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
declare ptr @last(ptr, ptr) #0
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, double 0.0, ptr @last)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

;--- callback-cast.ll
@name = private unnamed_addr constant [7 x i8] c"reduce\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.array(i32, ...)
define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}
define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %cast_as1 = addrspacecast ptr @last to ptr addrspace(1)
  %cast = addrspacecast ptr addrspace(1) %cast_as1 to ptr
  %result = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %array, double 0.0, ptr %cast)
  ret i32 0
}
attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }
