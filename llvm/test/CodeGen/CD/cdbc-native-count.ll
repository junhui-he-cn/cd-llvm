; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@count_name = private unnamed_addr constant [6 x i8] c"count\00"

declare ptr @llvm.cd.array(i32, ...)
declare double @llvm.cd.native(ptr, ...)
declare void @cd_print(double)

define i1 @yes(ptr %value) #0 {
entry:
  ret i1 true
}

define i1 @no(ptr %value) #0 {
entry:
  ret i1 false
}

define i32 @main() {
entry:
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %count_empty = call double (ptr, ...) @llvm.cd.native(
      ptr @count_name, ptr %empty, ptr @yes)
  call void @cd_print(double %count_empty)

  %source = call ptr (i32, ...) @llvm.cd.array(i32 3, i64 1, i64 2, i64 3)
  %count_all = call double (ptr, ...) @llvm.cd.native(
      ptr @count_name, ptr %source, ptr @yes)
  call void @cd_print(double %count_all)
  %count_none = call double (ptr, ...) @llvm.cd.native(
      ptr @count_name, ptr %source, ptr @no)
  call void @cd_print(double %count_none)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

; DIRECT: cdbc 0.2
; DIRECT: main registers=
; DIRECT-COUNT-3: call_native i0
; DIRECT: function f0 name="yes" arity=1
; DIRECT: function f1 name="no" arity=1
; MACHINE: cdbc 0.2
; MACHINE: main registers=
; MACHINE-COUNT-3: call_native i0
; MACHINE: function f0 name="yes" arity=1
; MACHINE: function f1 name="no" arity=1
