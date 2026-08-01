; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - | FileCheck %s

declare void @cd_print(i32)

define i32 @choose_select(i1 %condition) {
entry:
  %value = select i1 %condition, i32 7, i32 9
  ret i32 %value
}

define i32 @main() {
entry:
  %first = call i32 @choose_select(i1 true)
  call void @cd_print(i32 %first)
  %second = call i32 @choose_select(i1 false)
  call void @cd_print(i32 %second)
  ret i32 0
}

; CHECK: cdbc 0.1
; CHECK: main registers=
; CHECK: function f0 name="choose_select" arity=1 registers=
; CHECK: jump_if_false
; CHECK: move
; CHECK: jump
; CHECK: move
; CHECK: return
