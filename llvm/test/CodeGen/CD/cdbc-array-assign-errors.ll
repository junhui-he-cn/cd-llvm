; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/collection-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=COLLECTION-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/collection-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=COLLECTION-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/value-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=VALUE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/value-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=VALUE-MACHINE
; RUN: not llc -mtriple=cd-unknown-unknown %t/index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=INDEX-TYPE-DIRECT
; RUN: not llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=INDEX-TYPE-MACHINE

; COLLECTION-DIRECT: CD target does not support LLVM operation: llvm.cd.assign.index requires a CD dynamic-value collection
; COLLECTION-MACHINE: CD machine backend does not support llvm.cd.assign.index requires a CD dynamic-value collection
; VALUE-DIRECT: CD target does not support LLVM operation: llvm.cd.assign.index requires a scalar, nil, or CD dynamic-value assigned value
; VALUE-MACHINE: CD machine backend does not support llvm.cd.assign.index requires a scalar, nil, or CD dynamic-value assigned value
; INDEX-TYPE-DIRECT: intrinsic argument 1 type expected double, but got i64
; INDEX-TYPE-MACHINE: intrinsic argument 1 type expected double, but got i64

;--- collection-pointer.ll
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.assign.index(ptr, double, ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call ptr @llvm.cd.assign.index(ptr %slot, double 0.0, ptr null)
  ret i32 0
}

;--- value-pointer.ll
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.assign.index(ptr, double, ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call ptr @llvm.cd.assign.index(ptr %array, double 0.0, ptr %slot)
  ret i32 0
}

;--- index-type.ll
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.assign.index(ptr, i64, ptr)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call ptr @llvm.cd.assign.index(ptr %array, i64 0, ptr null)
  ret i32 0
}
