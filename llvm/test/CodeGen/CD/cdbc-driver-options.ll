; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s --check-prefix=O0
; RUN: llc -mtriple=cd-unknown-unknown -O2 %s -o - | FileCheck %s --check-prefix=O2
; RUN: not llc -mtriple=cd-unknown-unknown -g %s -o - 2>&1 | FileCheck %s --check-prefix=DEBUG
; RUN: not llc -mtriple=cd-unknown-unknown -filetype=obj %s -o - 2>&1 | FileCheck %s --check-prefix=OBJECT

define i32 @main() {
entry:
  ret i32 0
}

; O0: cdbc 0.1
; O0: main registers=
; O2: cdbc 0.1
; O2: main registers=
; DEBUG: llc: Unknown command line argument '-g'
; OBJECT: target does not support generation of this file type
