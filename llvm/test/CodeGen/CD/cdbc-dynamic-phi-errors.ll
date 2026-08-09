; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/ordinary.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/ordinary.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/foreign.ll -o - 2>&1 | FileCheck %s --check-prefix=FOREIGN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/foreign.ll -o - 2>&1 | FileCheck %s --check-prefix=FOREIGN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/mixed.ll -o - 2>&1 | FileCheck %s --check-prefix=MIXED-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/mixed.ll -o - 2>&1 | FileCheck %s --check-prefix=MIXED-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/undef.ll -o - 2>&1 | FileCheck %s --check-prefix=UNDEF-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/undef.ll -o - 2>&1 | FileCheck %s --check-prefix=UNDEF-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/poison.ll -o - 2>&1 | FileCheck %s --check-prefix=POISON-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/poison.ll -o - 2>&1 | FileCheck %s --check-prefix=POISON-MACHINE

; ORDINARY-DIRECT: CD target does not support LLVM instruction: phi
; ORDINARY-MACHINE: CD machine backend does not support a non-scalar PHI node
; FOREIGN-DIRECT: CD target does not support LLVM instruction: phi
; FOREIGN-MACHINE: CD machine backend does not support a non-scalar PHI node
; MIXED-DIRECT: CD target does not support LLVM instruction: phi
; MIXED-MACHINE: CD machine backend does not support a non-scalar PHI node
; UNDEF-DIRECT: CD target does not support LLVM instruction: phi
; UNDEF-MACHINE: CD machine backend does not support a non-scalar PHI node
; POISON-DIRECT: CD target does not support LLVM instruction: phi
; POISON-MACHINE: CD machine backend does not support a non-scalar PHI node

;--- ordinary.ll
define i32 @bad(i1 %condition) {
entry:
  br i1 %condition, label %left, label %right

left:
  br label %merge

right:
  br label %merge

merge:
  %selected = phi ptr [ inttoptr (i64 1 to ptr), %left ],
      [ inttoptr (i64 2 to ptr), %right ]
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad(i1 true)
  ret i32 %result
}

;--- foreign.ll
define i32 @bad(i1 %condition) {
entry:
  br i1 %condition, label %left, label %right

left:
  br label %merge

right:
  br label %merge

merge:
  %selected = phi ptr addrspace(1) [ null, %left ], [ null, %right ]
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad(i1 true)
  ret i32 %result
}

;--- mixed.ll
define i32 @bad(i1 %condition) {
entry:
  br i1 %condition, label %left, label %right

left:
  br label %merge

right:
  br label %merge

merge:
  %selected = phi ptr [ null, %left ],
      [ inttoptr (i64 2 to ptr), %right ]
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad(i1 true)
  ret i32 %result
}

;--- undef.ll
define i32 @bad(i1 %condition) {
entry:
  br i1 %condition, label %left, label %right

left:
  br label %merge

right:
  br label %merge

merge:
  %selected = phi ptr [ undef, %left ], [ null, %right ]
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad(i1 true)
  ret i32 %result
}

;--- poison.ll
define i32 @bad(i1 %condition) {
entry:
  br i1 %condition, label %left, label %right

left:
  br label %merge

right:
  br label %merge

merge:
  %selected = phi ptr [ poison, %left ], [ null, %right ]
  ret i32 0
}

define i32 @main() {
entry:
  %result = call i32 @bad(i1 true)
  ret i32 %result
}
