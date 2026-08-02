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

// A map is a fresh mutable VM value with alternating key/value operands.
def int_cd_map : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<0>>]>;

// A struct carries an optional type-name global followed by field-name/value
// pairs.  The names are ABI metadata, not CD string values.
def int_cd_struct : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_ptr_ty, llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<1>>]>;

// An enum variant carries explicit enum/variant name metadata followed by an
// ordered payload.  The names are not CD string values.
def int_cd_variant : DefaultAttrsIntrinsic<
    [llvm_ptr_ty], [llvm_ptr_ty, llvm_ptr_ty, llvm_i32_ty, llvm_vararg_ty],
    [ImmArg<ArgIndex<2>>]>;

def int_cd_variant_tag : DefaultAttrsIntrinsic<
    [llvm_i1_ty], [llvm_ptr_ty, llvm_ptr_ty, llvm_ptr_ty]>;

def int_cd_variant_field : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_i32_ty],
    [ImmArg<ArgIndex<1>>]>;

// A bounded native call carries an allowlisted name-table identity followed
// by operands whose types are validated by CDValueABI.
def int_cd_native : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_vararg_ty]>;

def int_cd_field : DefaultAttrsIntrinsic<[llvm_any_ty],
                                         [llvm_ptr_ty, llvm_ptr_ty]>;

def int_cd_assign_field : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_ptr_ty, LLVMMatchType<0>]>;

def int_cd_index : DefaultAttrsIntrinsic<[llvm_ptr_ty],
                                         [llvm_ptr_ty, llvm_double_ty]>;

def int_cd_assign_index : DefaultAttrsIntrinsic<
    [llvm_any_ty], [llvm_ptr_ty, llvm_double_ty, LLVMMatchType<0>]>;

def int_cd_len : DefaultAttrsIntrinsic<[llvm_double_ty], [llvm_ptr_ty]>;

def int_cd_assert_array : DefaultAttrsIntrinsic<[llvm_ptr_ty], [llvm_ptr_ty]>;
