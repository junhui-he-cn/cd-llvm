; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@contains_name = private unnamed_addr constant [9 x i8] c"contains\00"
@item = private unnamed_addr constant [5 x i8] c"item\00"

declare ptr @llvm.cd.array(i32, ...)
declare ptr @llvm.cd.map(i32, ...)
declare ptr @llvm.cd.string(ptr)
declare i1 @llvm.cd.native(ptr, ...)
declare void @cd_print(i1)

define i32 @main() {
entry:
  %needle = call ptr @llvm.cd.string(ptr @item)
  %array = call ptr (i32, ...) @llvm.cd.array(
      i32 2, i64 1, ptr %needle)
  %array_hit = call i1 (ptr, ...) @llvm.cd.native(
      ptr @contains_name, ptr %array, ptr %needle)
  %array_miss = call i1 (ptr, ...) @llvm.cd.native(
      ptr @contains_name, ptr %array, i64 2)

  %map = call ptr (i32, ...) @llvm.cd.map(
      i32 1, ptr %needle, i64 7)
  %map_hit = call i1 (ptr, ...) @llvm.cd.native(
      ptr @contains_name, ptr %map, ptr %needle)
  %map_miss = call i1 (ptr, ...) @llvm.cd.native(
      ptr @contains_name, ptr %map, i64 2)

  call void @cd_print(i1 %array_hit)
  call void @cd_print(i1 %array_miss)
  call void @cd_print(i1 %map_hit)
  call void @cd_print(i1 %map_miss)
  ret i32 0
}

; DIRECT: cdbc 0.1
; DIRECT-COUNT-4: native_call
; DIRECT-COUNT-4: print
; MACHINE: cdbc 0.1
; MACHINE-COUNT-4: native_call
; MACHINE-COUNT-4: print
