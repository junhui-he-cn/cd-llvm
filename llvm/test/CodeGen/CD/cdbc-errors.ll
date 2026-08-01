; RUN: not --crash llc -mtriple=cd-unknown-unknown -O0 %s -o - 2>&1 | FileCheck %s

define i32 @main() {
entry:
  switch i32 0, label %default [
    i32 1, label %one
  ]

one:
  ret i32 1

default:
  ret i32 0
}

; CHECK: CD target does not support LLVM instruction: switch
