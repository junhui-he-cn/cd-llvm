; RUN: llc -mtriple=cd-unknown-unknown -O0 %s -o - | FileCheck %s --check-prefix=O0
; RUN: llc -mtriple=cd-unknown-unknown -O2 %s -o - | FileCheck %s --check-prefix=LLC-O2
; RUN: opt -passes='default<O2>' %s -S -o - | llc -mtriple=cd-unknown-unknown -O0 -o - | FileCheck %s --check-prefix=O2

declare void @cd_print(i32)

define i32 @mem2reg_and_fold(i32 %input) #0 {
entry:
  %slot = alloca i32, align 4
  store i32 %input, ptr %slot, align 4
  %loaded = load i32, ptr %slot, align 4
  %base = add i32 40, 2
  %dead = add i32 %loaded, 0
  %result = add i32 %base, 0
  ret i32 %result
}

define i32 @select_after_cfg(i1 %condition) #0 {
entry:
  %value = select i1 %condition, i32 7, i32 9
  ret i32 %value
}

define i32 @main() {
entry:
  %folded = call i32 @mem2reg_and_fold(i32 5)
  call void @cd_print(i32 %folded)
  %selected = call i32 @select_after_cfg(i1 true)
  call void @cd_print(i32 %selected)
  ret i32 0
}

attributes #0 = { noinline }

; O0-LABEL: main registers=
; O0: print
; O0: print
; O0-LABEL: function f0 name="mem2reg_and_fold"
; O0: store_var
; O0: load_var
; O0: add
; O0: add
; O0: add
; O0: return
; O0-LABEL: function f1 name="select_after_cfg"
; O0: jump_if_false
; O0: move
; O0: jump
; O0: move
; O0: return

; O2-LABEL: main registers=
; O2: print
; O2: print
; O2-LABEL: function f0 name="mem2reg_and_fold"
; O2-NOT: store_var
; O2-NOT: add
; O2: load_var
; O2: constant
; O2: return
; O2-LABEL: function f1 name="select_after_cfg"
; O2: jump_if_false
; O2: move
; O2: jump
; O2: move
; O2: return

; LLC-O2: cdbc 0.1
; LLC-O2: main registers=
; LLC-O2: function f1 name="select_after_cfg"
