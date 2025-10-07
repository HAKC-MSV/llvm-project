// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_ANALYSIS_SERVER
// RUN: %HAKC_RUN_ANALYSIS_PASS
// sleep for n seconds to let server finish constructing dag
// RUN: sleep 15
// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE

// Test of custom transfer function (e.g., sk_buff should call hakc_transfer_skb)

struct sk_buff {
  unsigned int len;

};

int sk_buff_caller(struct sk_buff* buff) {
  buff->len = 1;
  return 0;
}

// CHECK-LABEL: @HAKC_XFER_sk_buff_caller
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK-NOT: call ptr @hakc_transfer_to_clique
// CHECK: call ptr @hakc_transfer_skb(ptr %0, i64 1, i64 13)
// CHECK: call i32 @HAKC_ORIG_sk_buff_caller(ptr %2)
// CHECK: call void @hakc_color_address(ptr %0, i32 %1, i64 4)
