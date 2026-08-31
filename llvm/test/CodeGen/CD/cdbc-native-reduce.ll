; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@reduce_name = private unnamed_addr constant [7 x i8] c"reduce\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.native(ptr, ...)
declare void @cd_print(ptr)

define ptr @last(ptr %accumulator, ptr %item) #0 {
entry:
  ret ptr %item
}

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %empty_result = call ptr (ptr, ...) @llvm.cd.native(
      ptr @reduce_name, ptr %empty, double 0.0, ptr @last)
  call void @cd_print(ptr %empty_result)

  %source = call ptr (i32, ...) @llvm.cd.array(
      i32 3, double 1.0, double 2.0, double 3.0)
  %reduced = call ptr (ptr, ...) @llvm.cd.native(
      ptr @reduce_name, ptr %source, double 0.0, ptr @last)
  call void @cd_print(ptr %reduced)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0,1" "cd.value.return" }

; DIRECT: cdbc 0.2
; DIRECT: main registers=
; DIRECT: make_function f0
; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: make_function f0
; DIRECT: call_native i0
; DIRECT: call_native i1
; DIRECT: function f0 name="last" arity=2
; DIRECT: param 0 = "accumulator"
; DIRECT: param 1 = "item"
; DIRECT: return
; MACHINE: cdbc 0.2
; MACHINE: main registers=
; MACHINE: make_function f0
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: make_function f0
; MACHINE: call_native i0
; MACHINE: call_native i1
; MACHINE: function f0 name="last" arity=2
; MACHINE: param 0 = "accumulator"
; MACHINE: param 1 = "item"
; MACHINE: return
