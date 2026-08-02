; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/missing.ll -o - 2>&1 | FileCheck %s --check-prefix=MISSING-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/missing.ll -o - 2>&1 | FileCheck %s --check-prefix=MISSING-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/duplicate.ll -o - 2>&1 | FileCheck %s --check-prefix=DUPLICATE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/duplicate.ll -o - 2>&1 | FileCheck %s --check-prefix=DUPLICATE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/shape.ll -o - 2>&1 | FileCheck %s --check-prefix=SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/entry-type.ll -o - 2>&1 | FileCheck %s --check-prefix=ENTRY-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/entry-type.ll -o - 2>&1 | FileCheck %s --check-prefix=ENTRY-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/non-entry-order.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-ENTRY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/non-entry-order.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-ENTRY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/dependency-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=DEPENDENCY-SHAPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/dependency-shape.ll -o - 2>&1 | FileCheck %s --check-prefix=DEPENDENCY-SHAPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/dependency-kind.ll -o - 2>&1 | FileCheck %s --check-prefix=DEPENDENCY-KIND-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/dependency-kind.ll -o - 2>&1 | FileCheck %s --check-prefix=DEPENDENCY-KIND-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-artifact=module %t/dependency-negative.ll -o - 2>&1 | FileCheck %s --check-prefix=DEPENDENCY-NEGATIVE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %t/dependency-negative.ll -o - 2>&1 | FileCheck %s --check-prefix=DEPENDENCY-NEGATIVE-MACHINE

; MISSING-DIRECT: CD target does not support LLVM operation: module artifact requires !cd.module metadata
; MISSING-MACHINE: CD machine backend does not support module artifact requires !cd.module metadata
; DUPLICATE-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.module must contain exactly one record
; DUPLICATE-MACHINE: CD machine backend does not support module metadata llvm.cd.module must contain exactly one record
; SHAPE-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.module record 0 must contain four or five operands
; SHAPE-MACHINE: CD machine backend does not support module metadata llvm.cd.module record 0 must contain four or five operands
; ENTRY-TYPE-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.module record 0 operand 3 must be an i1 constant
; ENTRY-TYPE-MACHINE: CD machine backend does not support module metadata llvm.cd.module record 0 operand 3 must be an i1 constant
; NON-ENTRY-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.module record 0 entry_order requires entry=true
; NON-ENTRY-MACHINE: CD machine backend does not support module metadata llvm.cd.module record 0 entry_order requires entry=true
; DEPENDENCY-SHAPE-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.dependencies record 0 must contain four operands
; DEPENDENCY-SHAPE-MACHINE: CD machine backend does not support module metadata llvm.cd.dependencies record 0 must contain four operands
; DEPENDENCY-KIND-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.dependencies record 0 has unsupported dependency kind
; DEPENDENCY-KIND-MACHINE: CD machine backend does not support module metadata llvm.cd.dependencies record 0 has unsupported dependency kind
; DEPENDENCY-NEGATIVE-DIRECT: CD target does not support LLVM operation: module metadata llvm.cd.dependencies record 0 operand 2 must be a non-negative 64-bit integer
; DEPENDENCY-NEGATIVE-MACHINE: CD machine backend does not support module metadata llvm.cd.dependencies record 0 operand 2 must be a non-negative 64-bit integer

;--- missing.ll
define i32 @main() {
entry:
  ret i32 0
}

;--- duplicate.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0, !1}
!0 = !{!"one", !"one.cd", !"/one.cd", i1 false}
!1 = !{!"two", !"two.cd", !"/two.cd", i1 false}

;--- shape.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0}
!0 = !{!"one", !"one.cd", !"/one.cd"}

;--- entry-type.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0}
!0 = !{!"one", !"one.cd", !"/one.cd", i32 1}

;--- non-entry-order.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0}
!0 = !{!"one", !"one.cd", !"/one.cd", i1 false, i64 0}

;--- dependency-shape.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0}
!0 = !{!"one", !"one.cd", !"/one.cd", i1 false}
!cd.dependencies = !{!1}
!1 = !{!"import", !"lib"}

;--- dependency-kind.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0}
!0 = !{!"one", !"one.cd", !"/one.cd", i1 false}
!cd.dependencies = !{!1}
!1 = !{!"link", !"lib", i64 0, !"./lib.cd"}

;--- dependency-negative.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.module = !{!0}
!0 = !{!"one", !"one.cd", !"/one.cd", i1 false}
!cd.dependencies = !{!1}
!1 = !{!"import", !"lib", i64 -1, !"./lib.cd"}
