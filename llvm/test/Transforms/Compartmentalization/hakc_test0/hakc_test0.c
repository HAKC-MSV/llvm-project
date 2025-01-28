// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE

struct data_struct {
    int a;
};

int bar(struct data_struct *);

int foo(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}
// todo: add better checking, maybe of t.ll after dag too?

// CHECK-LABEL: HAKC_ORIG_foo
// CHECK: %2 = icmp eq %struct.data_struct* %0, null

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %5 = call i8* @hakc_transfer_to_clique(i8* %4, i64 4, i32 6, i32 241, i1 false)
