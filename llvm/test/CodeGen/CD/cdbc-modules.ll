; RUN: llc -mtriple=cd-unknown-unknown -cd-artifact=module %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine -cd-artifact=module %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine
; RUN: not --crash llc -mtriple=cd-unknown-unknown %s -o - 2>&1 | FileCheck %s --check-prefix=PROGRAM-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - 2>&1 | FileCheck %s --check-prefix=PROGRAM-MACHINE

define i32 @main() {
entry:
  ret i32 7
}

!cd.module = !{!0}
!0 = !{!"entry", !"entry.cd", !"/workspace/entry.cd", i1 true, i64 0}
!cd.dependencies = !{!1}
!1 = !{!"import", !"/workspace/lib.cd", i64 1, !"./lib.cd"}

; DIRECT: cdbc 0.1
; DIRECT: artifact: module
; DIRECT: module:
; DIRECT: identity = "entry"
; DIRECT: path = "entry.cd"
; DIRECT: canonical_path = "/workspace/entry.cd"
; DIRECT: entry = true
; DIRECT: entry_order = 0
; DIRECT: dependencies:
; DIRECT: d0 target="/workspace/lib.cd" kind=import at=1 requested="./lib.cd"
; DIRECT: constants:
; DIRECT: main registers=
; DIRECT: return r{{[0-9]+}}

; MACHINE: cdbc 0.1
; MACHINE: artifact: module
; MACHINE: module:
; MACHINE: identity = "entry"
; MACHINE: path = "entry.cd"
; MACHINE: canonical_path = "/workspace/entry.cd"
; MACHINE: entry = true
; MACHINE: entry_order = 0
; MACHINE: dependencies:
; MACHINE: d0 target="/workspace/lib.cd" kind=import at=1 requested="./lib.cd"
; MACHINE: constants:
; MACHINE: main registers=
; MACHINE: return r{{[0-9]+}}

; PROGRAM-DIRECT: CD target does not support LLVM operation: module metadata requires -cd-artifact=module
; PROGRAM-MACHINE: CD machine backend does not support module metadata requires -cd-artifact=module
