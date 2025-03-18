// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_RUN_DAG_PASS
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_RUN_PYTHON_DAG
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE
// Testing if foo can find valid targets of bar and baz

int foo(int* _a);

int bar (int* _b);

int baz (int* _c);

int foo(int* _a) {
  int b = bar(_a);
  int c = baz(_a);
  return b + c;
}

// CHECK-LABEL: @HAKC_XFER_foo
// CHECK: call i32 @get_hakc_address_color(ptr %0)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 32, i64 3, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %3)
// CHECK: call void @hakc_color_address(ptr %0, i32 %2, i64 32)
