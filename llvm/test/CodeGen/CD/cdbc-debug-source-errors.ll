; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/one.ll -o - 2>&1 | FileCheck %s --check-prefix=ONE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/one.ll -o - 2>&1 | FileCheck %s --check-prefix=ONE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/four.ll -o - 2>&1 | FileCheck %s --check-prefix=FOUR-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/four.ll -o - 2>&1 | FileCheck %s --check-prefix=FOUR-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/non-string.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-STRING-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/non-string.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-STRING-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/empty-path.ll -o - 2>&1 | FileCheck %s --check-prefix=EMPTY-PATH-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/empty-path.ll -o - 2>&1 | FileCheck %s --check-prefix=EMPTY-PATH-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/empty-module.ll -o - 2>&1 | FileCheck %s --check-prefix=EMPTY-MODULE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/empty-module.ll -o - 2>&1 | FileCheck %s --check-prefix=EMPTY-MODULE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/invalid-utf8.ll -o - 2>&1 | FileCheck %s --check-prefix=INVALID-UTF8-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/invalid-utf8.ll -o - 2>&1 | FileCheck %s --check-prefix=INVALID-UTF8-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/duplicate.ll -o - 2>&1 | FileCheck %s --check-prefix=DUPLICATE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/duplicate.ll -o - 2>&1 | FileCheck %s --check-prefix=DUPLICATE-MACHINE

; ONE-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 0 must contain two or three string operands
; ONE-MACHINE: CD machine backend does not support llvm.cd.sources record 0 must contain two or three string operands
; FOUR-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 0 must contain two or three string operands
; FOUR-MACHINE: CD machine backend does not support llvm.cd.sources record 0 must contain two or three string operands
; NON-STRING-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 0 operand 0 must be a string
; NON-STRING-MACHINE: CD machine backend does not support llvm.cd.sources record 0 operand 0 must be a string
; EMPTY-PATH-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 0 path must not be empty
; EMPTY-PATH-MACHINE: CD machine backend does not support llvm.cd.sources record 0 path must not be empty
; EMPTY-MODULE-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 0 module identity must not be empty
; EMPTY-MODULE-MACHINE: CD machine backend does not support llvm.cd.sources record 0 module identity must not be empty
; INVALID-UTF8-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 0 contains invalid UTF-8
; INVALID-UTF8-MACHINE: CD machine backend does not support llvm.cd.sources record 0 contains invalid UTF-8
; DUPLICATE-DIRECT: CD target does not support LLVM operation: llvm.cd.sources record 1 duplicates source identity
; DUPLICATE-MACHINE: CD machine backend does not support llvm.cd.sources record 1 duplicates source identity

;--- one.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0}
!0 = !{!"only"}

;--- four.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0}
!0 = !{!"module", !"path", !"text", !"extra"}

;--- non-string.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0}
!0 = !{i32 1, !"text"}

;--- empty-path.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0}
!0 = !{!"", !"text"}

;--- empty-module.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0}
!0 = !{!"", !"path", !"text"}

;--- invalid-utf8.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0}
!0 = !{!"path\FF", !"text"}

;--- duplicate.ll
define i32 @main() {
entry:
  ret i32 0
}
!cd.sources = !{!0, !1}
!0 = !{!"demo.cd", !"one"}
!1 = !{!"demo.cd", !"two"}
