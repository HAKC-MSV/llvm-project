// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE

// Testing if foo can find valid targets of bar and baz

int foo(int _a);

int bar (int _b);

int baz (int _c);

int foo(int _a) {
  int b = bar(_a);
  int c = baz(_a);
  return b + c;
}

// todo: add better checking, maybe of t.ll after dag too?

// CHECK-LABEL: HAKC_ORIG_foo
// CHECK: %2 = icmp eq %struct.data_struct* %0, null

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %5 = call i8* @hakc_transfer_to_clique(i8* %4, i64 4, i32 6, i32 241, i1 false)
