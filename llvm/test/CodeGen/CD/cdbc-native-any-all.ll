; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@any_name = private unnamed_addr constant [4 x i8] c"any\00"
@all_name = private unnamed_addr constant [4 x i8] c"all\00"

declare ptr @llvm.cd.array(i32, ...)
declare i1 @llvm.cd.native(ptr, ...)
declare void @cd_print(i1)

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
  %any_empty = call i1 (ptr, ...) @llvm.cd.native(
      ptr @any_name, ptr %empty, ptr @yes)
  call void @cd_print(i1 %any_empty)

  %source = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 1, i64 2)
  %any_match = call i1 (ptr, ...) @llvm.cd.native(
      ptr @any_name, ptr %source, ptr @yes)
  call void @cd_print(i1 %any_match)

  %all_empty = call i1 (ptr, ...) @llvm.cd.native(
      ptr @all_name, ptr %empty, ptr @no)
  call void @cd_print(i1 %all_empty)
  %all_reject = call i1 (ptr, ...) @llvm.cd.native(
      ptr @all_name, ptr %source, ptr @no)
  call void @cd_print(i1 %all_reject)
  %all_match = call i1 (ptr, ...) @llvm.cd.native(
      ptr @all_name, ptr %source, ptr @yes)
  call void @cd_print(i1 %all_match)
  ret i32 0
}

attributes #0 = { "cd.value.params"="0" }

; DIRECT: cdbc 0.2
; DIRECT: main registers=
; DIRECT-COUNT-2: call_native i0
; DIRECT-COUNT-3: call_native i2
; DIRECT: function f0 name="yes" arity=1
; DIRECT: function f1 name="no" arity=1
; MACHINE: cdbc 0.2
; MACHINE: main registers=
; MACHINE-COUNT-2: call_native i0
; MACHINE-COUNT-3: call_native i2
; MACHINE: function f0 name="yes" arity=1
; MACHINE: function f1 name="no" arity=1
