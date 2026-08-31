; RUN: llc -mtriple=cd-unknown-unknown %s -o %t.direct
; RUN: FileCheck --check-prefix=DIRECT %s < %t.direct
; RUN: llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o %t.machine
; RUN: FileCheck --check-prefix=MACHINE %s < %t.machine

@message = private unnamed_addr constant [12 x i8] c"hello\0Aworld\00"
@escaped = private unnamed_addr constant [6 x i8] c"a\22b\5Cc\00"
@empty = private unnamed_addr constant [1 x i8] c"\00"
@utf8 = private unnamed_addr constant [7 x i8] c"\E4\BD\A0\E5\A5\BD\00"

declare ptr @llvm.cd.string(ptr)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %first = call ptr @llvm.cd.string(ptr @message)
  call void @cd_print(ptr %first)
  %second = call ptr @llvm.cd.string(ptr @message)
  call void @cd_print(ptr %second)
  %third = call ptr @llvm.cd.string(ptr @escaped)
  call void @cd_print(ptr %third)
  %fourth = call ptr @llvm.cd.string(ptr @empty)
  call void @cd_print(ptr %fourth)
  %fifth = call ptr @llvm.cd.string(ptr @utf8)
  call void @cd_print(ptr %fifth)
  ret i32 0
}

; DIRECT: c0 = string "hello\nworld"
; DIRECT: c1 = string "a\"b\\c"
; DIRECT: c2 = string ""
; DIRECT: c3 = string "你好"
; DIRECT: call_native i0
; MACHINE: c0 = string "hello\nworld"
; MACHINE: c1 = string "a\"b\\c"
; MACHINE: c2 = string ""
; MACHINE: c3 = string "你好"
; MACHINE: call_native i0
