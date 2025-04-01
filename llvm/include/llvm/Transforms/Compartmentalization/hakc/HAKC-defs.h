//
// Created by derrick on 8/20/21.
//

#ifndef PMC_HAKC_DEFS_H
#define PMC_HAKC_DEFS_H

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"

/* Macro value defined in CheriBSD sys/module.h */
#define HAKC_CHERIBSD_COMPARTMENT_METADATA_TYPE 5
#define HACK_CHERIBSD_DEFAULT_VERSION 1

#define HAKC_CONTEXT_COMPARTMENT_SHIFT 16

#define DIVISION_ID_BIT_LENGTH 64
#define COMPARTMENT_ID_BIT_LENGTH 64
#define ENTRY_TOKEN_BIT_LENGTH 64
#define EPOCH_ID_LENGTH 64

constexpr size_t BITS_PER_BYTE = 8;

namespace llvm::hakc {
typedef uint64_t hakc_compartment_id_t;
typedef uint64_t hakc_access_token_t;
typedef uint64_t hakc_compartment_division_t;
typedef uint64_t hakc_arg_t;
typedef std::string hakc_label_t;
typedef StringRef hakc_label_ref_t;
typedef uint64_t tictac_epoch_id_t;

typedef ConstantInt *HAKC_Compartment_ID;
typedef ConstantInt *HAKC_Access_Token;
typedef ConstantInt *HAKC_Division_ID;

const StringRef OUTSIDE_TRANSFER_PREFIX = "HAKC_XFER_";
const StringRef ORIGINAL_FUNCTION_PREFIX = "HAKC_ORIG_";
const StringRef VARIADIC_TRANSFER_PREFIX = "HAKC_VARF_";
const StringRef CAPABILITY_REASSIGNMENT_PREFIX = "_hakc_reassignment_";
const StringRef MODPARAM_GETCTX_PREFIX = "hakc_modparam_getctx_";

const uint64_t user_space_end = 0x0000ffffffffffff;

const StringRef HAKC_SECTION_PREFIX = ".hakc.";
const StringRef HAKC_MODPARAM_TEXT_SECTION = ".hakc.modparam_ctx.text";
const StringRef HAKC_MODPARAM_FUNCP_SECTION = ".hakc.modparam_ctx_fp";

typedef enum {
  NO_CLIQUE,
  GREEN_CLIQUE,
  RED_CLIQUE,
  ORANGE_CLIQUE,
  YELLOW_CLIQUE,
  PURPLE_CLIQUE,
  BLUE_CLIQUE,
  GREY_CLIQUE,
  PINK_CLIQUE,
  BROWN_CLIQUE,
  WHITE_CLIQUE,
  BLACK_CLIQUE,
  TEAL_CLIQUE,
  VIOLET_CLIQUE,
  CRIMSON_CLIQUE,
  GOLD_CLIQUE,
} sym_color_t;

typedef enum { hakc_global_scope, hakc_local_scope } hakc_scope_t;
typedef enum {
  READ_ONLY = 0,
  READ_WRITE = 1
} epoch_perms_options_t;

const hakc_compartment_id_t KERNEL_COMPARTMENT = 0;
const tictac_epoch_id_t KERNEL_EPOCH = 0;
const hakc_compartment_division_t KERNEL_DIVISION = NO_CLIQUE;
const hakc_access_token_t KERNEL_ACCESS_TOKEN = 0;
} // namespace llvm::hakc
#endif // PMC_HAKC_DEFS_H
