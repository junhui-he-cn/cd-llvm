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
; O0: call_native i0
; O0: call_native i0
; O0-LABEL: function f0 name="mem2reg_and_fold"
; O0: {{(bind_local|set_local|init_global|set_global)}}
; O0: {{(load_local|load_global)}}
; O0: add
; O0: add
; O0: add
; O0: return
; O0-LABEL: function f1 name="select_after_cfg"
; O0: br_if
; O0: move
; O0: br
; O0: move
; O0: return

; O2-LABEL: main registers=
; O2: call_native i0
; O2: call_native i0
; O2-LABEL: function f0 name="mem2reg_and_fold"
; O2-NOT: {{(bind_local|set_local|init_global|set_global)}}
; O2-NOT: add
; O2: {{(load_local|load_global)}}
; O2: constant
; O2: return
; O2-LABEL: function f1 name="select_after_cfg"
; O2: br_if
; O2: move
; O2: br
; O2: move
; O2: return

; LLC-O2: cdbc 0.2
; LLC-O2: main registers=
; LLC-O2: function f1 name="select_after_cfg"
