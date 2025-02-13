// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE
struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int foo(struct data_struct2 *a) {
    if (a) {
        struct data_struct b;
        b.a = 0;
        return a->f(&b);
    }
    return 0;
}

// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %7 = call i32 @HAKC_ORIG_foo(%struct.data_struct2* %6)
