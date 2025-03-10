// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
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

// todo: add better checking, maybe of t.ll after dag too?

// CHECK: @foo = alias i32 (ptr), ptr @HAKC_XFER_foo

// CHECK-LABEL: HAKC_ORIG_foo
// CHECK: call i32 @bar(ptr noundef %5)

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 3, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %1)
