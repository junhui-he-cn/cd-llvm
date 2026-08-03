; RUN: split-file %s %t
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/unknown-name.ll -o - 2>&1 | FileCheck %s --check-prefix=UNKNOWN-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/unknown-name.ll -o - 2>&1 | FileCheck %s --check-prefix=UNKNOWN-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/callback-name.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/callback-name.ll -o - 2>&1 | FileCheck %s --check-prefix=CALLBACK-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/floor-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/floor-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/floor-type.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/floor-type.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/string-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=STRING-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/string-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=STRING-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/bad-name-global.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-NAME-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/bad-name-global.ll -o - 2>&1 | FileCheck %s --check-prefix=BAD-NAME-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/range-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=RANGE-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/range-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=RANGE-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/floor-result.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/floor-result.ll -o - 2>&1 | FileCheck %s --check-prefix=FLOOR-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-INDEX-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-INDEX-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-SCALAR-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-SCALAR-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/substr-result.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/substr-result.ll -o - 2>&1 | FileCheck %s --check-prefix=SUBSTR-RESULT-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-ARITY-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-arity.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-ARITY-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-INDEX-TYPE-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-index-type.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-INDEX-TYPE-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-SCALAR-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-scalar.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-SCALAR-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-POINTER-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-pointer.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-POINTER-MACHINE
; RUN: not --crash llc -mtriple=cd-unknown-unknown %t/char-at-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-RESULT-DIRECT
; RUN: not --crash llc -mtriple=cd-unknown-unknown -cd-backend=machine %t/char-at-result.ll -o - 2>&1 | FileCheck %s --check-prefix=CHAR-AT-RESULT-MACHINE

; UNKNOWN-DIRECT: CD target does not support LLVM operation: llvm.cd.native native name is not supported by the bounded CD ABI: mystery
; UNKNOWN-MACHINE: CD machine backend does not support llvm.cd.native native name is not supported by the bounded CD ABI: mystery
; CALLBACK-DIRECT: CD target does not support LLVM operation: llvm.cd.native native name is not supported by the bounded CD ABI: map
; CALLBACK-MACHINE: CD machine backend does not support llvm.cd.native native name is not supported by the bounded CD ABI: map
; FLOOR-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native floor requires one double argument and a double result
; FLOOR-ARITY-MACHINE: CD machine backend does not support llvm.cd.native floor requires one double argument and a double result
; FLOOR-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native floor requires one double argument and a double result
; FLOOR-TYPE-MACHINE: CD machine backend does not support llvm.cd.native floor requires one double argument and a double result
; STRING-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native str requires one scalar or CD dynamic-value argument and a ptr result
; STRING-POINTER-MACHINE: CD machine backend does not support llvm.cd.native str requires one scalar or CD dynamic-value argument and a ptr result
; BAD-NAME-DIRECT: CD target does not support LLVM operation: llvm.cd.native requires a private non-empty string global native name
; BAD-NAME-MACHINE: CD machine backend does not support llvm.cd.native requires a private non-empty string global native name
; RANGE-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native range requires one to three double arguments and a ptr result
; RANGE-ARITY-MACHINE: CD machine backend does not support llvm.cd.native range requires one to three double arguments and a ptr result
; FLOOR-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native floor requires one double argument and a double result
; FLOOR-RESULT-MACHINE: CD machine backend does not support llvm.cd.native floor requires one double argument and a double result
; SUBSTR-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-ARITY-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-INDEX-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-INDEX-TYPE-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-SCALAR-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-SCALAR-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-POINTER-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; SUBSTR-RESULT-MACHINE: CD machine backend does not support llvm.cd.native substr requires a CD string value, two double arguments, and a ptr result
; CHAR-AT-ARITY-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-ARITY-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-INDEX-TYPE-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-INDEX-TYPE-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-SCALAR-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-SCALAR-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-POINTER-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-POINTER-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-RESULT-DIRECT: CD target does not support LLVM operation: llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result
; CHAR-AT-RESULT-MACHINE: CD machine backend does not support llvm.cd.native charAt requires a CD string value, one double argument, and a ptr result

;--- unknown-name.ll
@name = private unnamed_addr constant [8 x i8] c"mystery\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- callback-name.ll
@name = private unnamed_addr constant [4 x i8] c"map\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- floor-arity.ll
@name = private unnamed_addr constant [6 x i8] c"floor\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name)
  ret i32 0
}

;--- floor-type.ll
@name = private unnamed_addr constant [6 x i8] c"floor\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, i64 1)
  ret i32 0
}

;--- string-pointer.ll
@name = private unnamed_addr constant [4 x i8] c"str\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %slot)
  ret i32 0
}

;--- bad-name-global.ll
@name = global [6 x i8] c"floor\00"
declare double @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- range-arity.ll
@name = private unnamed_addr constant [6 x i8] c"range\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(
      ptr @name, double 1.0, double 2.0, double 3.0, double 4.0)
  ret i32 0
}

;--- floor-result.ll
@name = private unnamed_addr constant [6 x i8] c"floor\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, double 1.0)
  ret i32 0
}

;--- substr-arity.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, double 1.0)
  ret i32 0
}

;--- substr-index-type.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, i64 1, double 1.0)
  ret i32 0
}

;--- substr-scalar.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1, double 1.0, double 1.0)
  ret i32 0
}

;--- substr-pointer.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %slot, double 1.0, double 1.0)
  ret i32 0
}

;--- substr-result.ll
@name = private unnamed_addr constant [7 x i8] c"substr\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, double 1.0, double 1.0)
  ret i32 0
}

;--- char-at-arity.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source)
  ret i32 0
}

;--- char-at-index-type.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare ptr @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, i64 1)
  ret i32 0
}

;--- char-at-scalar.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, i64 1, double 1.0)
  ret i32 0
}

;--- char-at-pointer.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
declare ptr @llvm.cd.native(ptr, ...)
define i32 @main() {
entry:
  %slot = alloca i8
  %value = call ptr (ptr, ...) @llvm.cd.native(ptr @name, ptr %slot, double 1.0)
  ret i32 0
}

;--- char-at-result.ll
@name = private unnamed_addr constant [7 x i8] c"charAt\00"
@message = private unnamed_addr constant [6 x i8] c"hello\00"
declare double @llvm.cd.native(ptr, ...)
declare ptr @llvm.cd.string(ptr)
define i32 @main() {
entry:
  %source = call ptr @llvm.cd.string(ptr @message)
  %value = call double (ptr, ...) @llvm.cd.native(ptr @name, ptr %source, double 1.0)
  ret i32 0
}
