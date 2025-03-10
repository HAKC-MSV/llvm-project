// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
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

// CHECK-LABEL: define dso_local i32 @HAKC_XFER_foo(ptr noundef %0)
// CHECK-LABEL: HAKCTransferEntry:
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 1, i64 1, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %1)
