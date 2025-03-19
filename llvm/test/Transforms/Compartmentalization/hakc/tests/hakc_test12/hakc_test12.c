// RUN: source %HAKC_PYTHON_VENV/bin/activate
// RUN: %HAKC_START_POLICY_SERVER & sleep 1
// RUN: %hakc_clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config=%HAKC_CONFIG -o %t %s
// RUN: deactivate
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
// CHECK-NOT: @hakc_transfer_to_clique
// CHECK: call ptr @hakc_transfer_skb(ptr %0, i64 1, i64 13)
// CHECK: call i32 @HAKC_ORIG_sk_buff_caller(ptr %3)
// CHECK: call void @hakc_color_address(ptr %0, i32 %2, i64 32)
