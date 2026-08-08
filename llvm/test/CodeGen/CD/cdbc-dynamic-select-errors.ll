; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/ordinary.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/ordinary.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/foreign.ll -o - 2>&1 | FileCheck %s --check-prefix=FOREIGN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/foreign.ll -o - 2>&1 | FileCheck %s --check-prefix=FOREIGN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/mixed.ll -o - 2>&1 | FileCheck %s --check-prefix=MIXED-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/mixed.ll -o - 2>&1 | FileCheck %s --check-prefix=MIXED-MACHINE

; ORDINARY-DIRECT: CD target does not support LLVM instruction: select
; ORDINARY-MACHINE: CD machine backend does not support a non-scalar instruction result
; FOREIGN-DIRECT: CD target does not support LLVM instruction: select
; FOREIGN-MACHINE: CD machine backend does not support a non-scalar instruction result
; MIXED-DIRECT: CD target does not support LLVM instruction: select
; MIXED-MACHINE: CD machine backend does not support a non-scalar instruction result

;--- ordinary.ll
define i32 @main() {
entry:
  %selected = select i1 true, ptr inttoptr (i64 1 to ptr),
      ptr inttoptr (i64 2 to ptr)
  ret i32 0
}

;--- foreign.ll
define i32 @main() {
entry:
  %selected = select i1 true, ptr addrspace(1) null, ptr addrspace(1) null
  ret i32 0
}

;--- mixed.ll
define i32 @main() {
entry:
  %selected = select i1 true, ptr null, ptr inttoptr (i64 1 to ptr)
  ret i32 0
}
