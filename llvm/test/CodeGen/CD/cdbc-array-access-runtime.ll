; RUN: not --crash llc -mtriple=cd-unknown-unknown %s -o - 2>&1 | FileCheck --check-prefix=DIRECT %s
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %s -o - 2>&1 | FileCheck --check-prefix=MACHINE %s

declare ptr @llvm.cd.assert.array(ptr)
declare ptr @llvm.cd.array(i32, ...)
declare void @cd_print(ptr)

define i32 @main() {
entry:
  %array = call ptr (i32, ...) @llvm.cd.array(i32 1, i64 1)
  %value = call ptr @llvm.cd.assert.array(ptr %array)
  call void @cd_print(ptr %value)
  ret i32 0
}

; DIRECT: CD bytecode 0.2 lowering failed: llvm.cd.assert.array has no cdbc 0.2 opcode
; MACHINE: CD bytecode 0.2 lowering failed: llvm.cd.assert.array has no cdbc 0.2 opcode
