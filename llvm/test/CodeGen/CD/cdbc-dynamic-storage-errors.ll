; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/uninitialized.ll -o - 2>&1 | FileCheck %s --check-prefix=UNINITIALIZED-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/uninitialized.ll -o - 2>&1 | FileCheck %s --check-prefix=UNINITIALIZED-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/partial.ll -o - 2>&1 | FileCheck %s --check-prefix=PARTIAL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/partial.ll -o - 2>&1 | FileCheck %s --check-prefix=PARTIAL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/unproven.ll -o - 2>&1 | FileCheck %s --check-prefix=UNPROVEN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/unproven.ll -o - 2>&1 | FileCheck %s --check-prefix=UNPROVEN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/escaped.ll -o - 2>&1 | FileCheck %s --check-prefix=ESCAPED-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/escaped.ll -o - 2>&1 | FileCheck %s --check-prefix=ESCAPED-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/volatile.ll -o - 2>&1 | FileCheck %s --check-prefix=VOLATILE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/volatile.ll -o - 2>&1 | FileCheck %s --check-prefix=VOLATILE-MACHINE

; UNINITIALIZED-DIRECT: CD target does not support LLVM instruction: load
; UNINITIALIZED-MACHINE: CD machine backend does not support a non-scalar instruction result
; PARTIAL-DIRECT: CD target does not support LLVM instruction: load
; PARTIAL-MACHINE: CD machine backend does not support a non-scalar instruction result
; UNPROVEN-DIRECT: CD target does not support LLVM instruction: load
; UNPROVEN-MACHINE: CD machine backend does not support a non-scalar instruction result
; ESCAPED-DIRECT: CD target does not support LLVM instruction: alloca
; ESCAPED-MACHINE: CD machine backend does not support an unsupported alloca shape
; VOLATILE-DIRECT: CD target does not support LLVM instruction: store
; VOLATILE-MACHINE: CD machine backend does not support an unsupported store

;--- uninitialized.ll
define i32 @bad() {
entry:
  %slot = alloca ptr
  %value = load ptr, ptr %slot
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad()
  ret i32 %result
}

;--- partial.ll
define i32 @bad(i1 %condition) {
entry:
  %slot = alloca ptr
  br i1 %condition, label %initialized, label %uninitialized

initialized:
  store ptr null, ptr %slot
  br label %merge

uninitialized:
  br label %merge

merge:
  %value = load ptr, ptr %slot
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad(i1 true)
  ret i32 %result
}

;--- unproven.ll
define i32 @bad() {
entry:
  %slot = alloca ptr
  store ptr %slot, ptr %slot
  %value = load ptr, ptr %slot
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad()
  ret i32 %result
}

;--- escaped.ll
declare void @sink(ptr)

define i32 @bad() {
entry:
  %slot = alloca ptr
  call void @sink(ptr %slot)
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad()
  ret i32 %result
}

;--- volatile.ll
define i32 @bad() {
entry:
  %slot = alloca ptr
  store volatile ptr null, ptr %slot
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad()
  ret i32 %result
}
