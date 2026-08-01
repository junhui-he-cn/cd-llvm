//===- IntrinsicsCD.td - CD target intrinsics ----------------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.  See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// A CD string is an opaque target value carried in an LLVM ptr token.  The CD
// target consumes the pointed-to constant initializer while lowering; it is
// not a native memory read.
def int_cd_string : DefaultAttrsIntrinsic<[llvm_ptr_ty], [llvm_ptr_ty],
                                          [IntrNoMem]>;

// An array is a fresh mutable VM value.  Keep the conservative default memory
// and side-effect properties so LLVM cannot fold two constructors together.
def int_cd_array : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<0>>]>;

def int_cd_index : DefaultAttrsIntrinsic<[llvm_ptr_ty],
                                         [llvm_ptr_ty, llvm_double_ty]>;

def int_cd_assign_index : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_double_ty, LLVMMatchType<0>]>;

def int_cd_len : DefaultAttrsIntrinsic<[llvm_double_ty], [llvm_ptr_ty]>;

def int_cd_assert_array : DefaultAttrsIntrinsic<[llvm_ptr_ty], [llvm_ptr_ty]>;
