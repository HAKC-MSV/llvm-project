// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_COMP_DAG_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_RUN_PYTHON_DAG
// RUN: %HAKC_RUN_DAG_PASS
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

// Test of custom transfer function (e.g., sk_buff should call hakc_transfer_skb)

struct sk_buff {
  unsigned int len;

};

int sk_buff_caller(struct sk_buff* buff) {
  buff->len = 1;
  return 0;
}

// CHECK: @sk_buff_caller = alias i32 (ptr), ptr @HAKC_XFER_sk_buff_caller

// CHECK-LABEL: HAKC_ORIG_sk_buff_caller

// CHECK-LABEL: HAKC_XFER_sk_buff_caller

// CHECK NOT: @hakc_transfer_to_clique

// CHECK @hakc_transfer_skb
