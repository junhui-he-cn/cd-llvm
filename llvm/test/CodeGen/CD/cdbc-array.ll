; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@message = private unnamed_addr constant [6 x i8] c"hello\00"

declare ptr @llvm.cd.string(ptr)
declare ptr @llvm.cd.array(i32, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @message)
  %empty = call ptr (i32, ...) @llvm.cd.array(i32 0)
  %values = call ptr (i32, ...) @llvm.cd.array(i32 4, i64 1, i1 true, ptr %string, ptr null)
  %nested = call ptr (i32, ...) @llvm.cd.array(i32 2, i64 2, ptr %values)
  call void @cd_print(ptr %empty)
  call void @cd_print(ptr %values)
  call void @cd_print(ptr %nested)
  ret i32 0
}

; DIRECT: r{{[0-9]+}} = array []
; DIRECT: r{{[0-9]+}} = array [
; DIRECT: r{{[0-9]+}} = array [
; MACHINE: r{{[0-9]+}} = array []
; MACHINE: r{{[0-9]+}} = array [
; MACHINE: r{{[0-9]+}} = array [
