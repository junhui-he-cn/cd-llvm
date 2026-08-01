; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/count.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/count.ll -o - 2>&1 | FileCheck %s --check-prefix=COUNT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/key-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=KEY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/key-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=KEY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/value-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=VALUE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/value-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=VALUE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/foreign-null.ll -o - 2>&1 | FileCheck %s --check-prefix=NULL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/foreign-null.ll -o - 2>&1 | FileCheck %s --check-prefix=NULL-MACHINE

; COUNT-DIRECT: CD target does not support LLVM operation: llvm.cd.map entry-count does not match the key/value operand list
; COUNT-MACHINE: CD machine backend does not support llvm.cd.map entry-count does not match the key/value operand list
; KEY-DIRECT: CD target does not support LLVM operation: llvm.cd.map requires a scalar, nil, or string-token key operand
; KEY-MACHINE: CD machine backend does not support llvm.cd.map requires a scalar, nil, or string-token key operand
; VALUE-DIRECT: CD target does not support LLVM operation: llvm.cd.map requires a scalar, nil, or CD dynamic-value value operand
; VALUE-MACHINE: CD machine backend does not support llvm.cd.map requires a scalar, nil, or CD dynamic-value value operand
; NULL-DIRECT: CD target does not support LLVM operation: llvm.cd.map requires a scalar, nil, or string-token key operand
; NULL-MACHINE: CD machine backend does not support llvm.cd.map requires a scalar, nil, or string-token key operand

;--- count.ll
declare ptr @llvm.cd.map(i32, ...)

define i32 @main() {
entry:
  %map = call ptr (i32, ...) @llvm.cd.map(i32 2, i64 1, i64 10)
  ret i32 0
}

;--- key-pointer.ll
declare ptr @llvm.cd.map(i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %map = call ptr (i32, ...) @llvm.cd.map(i32 1, ptr %slot, i64 10)
  ret i32 0
}

;--- value-pointer.ll
declare ptr @llvm.cd.map(i32, ...)

define i32 @main() {
entry:
  %slot = alloca i8
  %map = call ptr (i32, ...) @llvm.cd.map(i32 1, i64 1, ptr %slot)
  ret i32 0
}

;--- foreign-null.ll
declare ptr @llvm.cd.map(i32, ...)

define i32 @main() {
entry:
  %map = call ptr (i32, ...) @llvm.cd.map(
      i32 1, ptr addrspace(1) null, i64 10)
  ret i32 0
}
