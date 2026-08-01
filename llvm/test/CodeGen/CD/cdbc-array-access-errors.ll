; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/index-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=INDEX-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/index-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=INDEX-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/len-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=LEN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/len-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=LEN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/assert-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ASSERT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/assert-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ASSERT-MACHINE
; RUN: not llc -mtriple=cd-unknown-unknown %t/index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=INDEX-TYPE-DIRECT
; RUN: not llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=INDEX-TYPE-MACHINE

; INDEX-DIRECT: CD target does not support LLVM operation: llvm.cd.index requires a CD dynamic-value collection
; INDEX-MACHINE: CD machine backend does not support llvm.cd.index requires a CD dynamic-value collection
; LEN-DIRECT: CD target does not support LLVM operation: llvm.cd.len requires a CD dynamic-value operand
; LEN-MACHINE: CD machine backend does not support llvm.cd.len requires a CD dynamic-value operand
; ASSERT-DIRECT: CD target does not support LLVM operation: llvm.cd.assert.array requires a CD dynamic-value operand
; ASSERT-MACHINE: CD machine backend does not support llvm.cd.assert.array requires a CD dynamic-value operand
; INDEX-TYPE-DIRECT: intrinsic argument 1 type expected double, but got i64
; INDEX-TYPE-MACHINE: intrinsic argument 1 type expected double, but got i64

;--- index-pointer.ll
declare ptr @llvm.cd.index(ptr, double)

define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr @llvm.cd.index(ptr %slot, double 0.0)
  ret i32 0
}

;--- len-pointer.ll
declare double @llvm.cd.len(ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %value = call double @llvm.cd.len(ptr %slot)
  ret i32 0
}

;--- assert-pointer.ll
declare ptr @llvm.cd.assert.array(ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr @llvm.cd.assert.array(ptr %slot)
  ret i32 0
}

;--- index-type.ll
declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.index(ptr, i64)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %value = call ptr @llvm.cd.index(ptr %array, i64 0)
  ret i32 0
}
