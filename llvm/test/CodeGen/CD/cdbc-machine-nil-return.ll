; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s
; RUN: llc -mtriple=cd-unknown-unknown %s -o - | FileCheck --check-prefix=DIRECT %s

define ptr @main() {
entry:
  ret ptr null
}

; CHECK: cdbc 0.1
; CHECK: constants:
; CHECK-NEXT:   c0 = nil
; CHECK: main registers=1:
; CHECK-NEXT:   r0 = constant c0
; CHECK-NEXT:   return r0
; DIRECT: cdbc 0.1
; DIRECT: c0 = nil
; DIRECT: return r0
