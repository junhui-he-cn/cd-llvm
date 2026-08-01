; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/mutable.ll -o - 2>&1 | FileCheck %s --check-prefix=MUTABLE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/mutable.ll -o - 2>&1 | FileCheck %s --check-prefix=MUTABLE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/non-private.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-PRIVATE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/non-private.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-PRIVATE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/non-string.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-STRING-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/non-string.ll -o - 2>&1 | FileCheck %s --check-prefix=NON-STRING-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/missing-nul.ll -o - 2>&1 | FileCheck %s --check-prefix=MISSING-NUL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/missing-nul.ll -o - 2>&1 | FileCheck %s --check-prefix=MISSING-NUL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/embedded-nul.ll -o - 2>&1 | FileCheck %s --check-prefix=EMBEDDED-NUL-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/embedded-nul.ll -o - 2>&1 | FileCheck %s --check-prefix=EMBEDDED-NUL-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/invalid-utf8.ll -o - 2>&1 | FileCheck %s --check-prefix=INVALID-UTF8-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/invalid-utf8.ll -o - 2>&1 | FileCheck %s --check-prefix=INVALID-UTF8-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/ordinary-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/ordinary-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=ORDINARY-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/string-compare.ll -o - 2>&1 | FileCheck %s --check-prefix=STRING-COMPARE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/string-compare.ll -o - 2>&1 | FileCheck %s --check-prefix=STRING-COMPARE-MACHINE

; MUTABLE-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires a private constant address-space-zero global
; MUTABLE-MACHINE: CD machine backend does not support llvm.cd.string requires a private constant address-space-zero global
; NON-PRIVATE-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires a private constant address-space-zero global
; NON-PRIVATE-MACHINE: CD machine backend does not support llvm.cd.string requires a private constant address-space-zero global
; NON-STRING-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires a nul-terminated byte string global
; NON-STRING-MACHINE: CD machine backend does not support llvm.cd.string requires a nul-terminated byte string global
; MISSING-NUL-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires a nul-terminated byte string global
; MISSING-NUL-MACHINE: CD machine backend does not support llvm.cd.string requires a nul-terminated byte string global
; EMBEDDED-NUL-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires a nul-terminated byte string global
; EMBEDDED-NUL-MACHINE: CD machine backend does not support llvm.cd.string requires a nul-terminated byte string global
; INVALID-UTF8-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires valid UTF-8 bytes
; INVALID-UTF8-MACHINE: CD machine backend does not support llvm.cd.string requires valid UTF-8 bytes
; ORDINARY-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.string requires a direct string global operand
; ORDINARY-POINTER-MACHINE: CD machine backend does not support llvm.cd.string requires a direct string global operand
; STRING-COMPARE-DIRECT: CD target does not support LLVM instruction: icmp
; STRING-COMPARE-MACHINE: CD machine backend does not support a non-scalar comparison

;--- mutable.ll
@value = private unnamed_addr global [2 x i8] c"a\00"

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @value)
  ret i32 0
}

;--- non-private.ll
@value = internal unnamed_addr constant [2 x i8] c"a\00"

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @value)
  ret i32 0
}

;--- non-string.ll
@value = private unnamed_addr constant [2 x i16] [i16 97, i16 0]

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @value)
  ret i32 0
}

;--- missing-nul.ll
@value = private unnamed_addr constant [1 x i8] c"a"

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @value)
  ret i32 0
}

;--- embedded-nul.ll
@value = private unnamed_addr constant [3 x i8] c"a\00b"

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @value)
  ret i32 0
}

;--- invalid-utf8.ll
@value = private unnamed_addr constant [2 x i8] c"\FF\00"

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %string = call ptr @llvm.cd.string(ptr @value)
  ret i32 0
}

;--- ordinary-pointer.ll
declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %slot = alloca i8
  %string = call ptr @llvm.cd.string(ptr %slot)
  ret i32 0
}

;--- string-compare.ll
@value = private unnamed_addr constant [2 x i8] c"a\00"

declare ptr @llvm.cd.string(ptr)

define i32 @main() {
entry:
  %left = call ptr @llvm.cd.string(ptr @value)
  %right = call ptr @llvm.cd.string(ptr @value)
  %equal = icmp eq ptr %left, %right
  ret i32 0
}
