// RUN: %HAKC_SETUP
// RUN: %HAKC_YAML_REPLACE_PATHS
// RUN: %HAKC_YAML_CHANGE_MODE_DAG
// RUN: %HAKC_YAML_CHANGE_MODE_COMP
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_PASS_DAG_ANALYSIS
// RUN: %HAKC_PYTHON_CREATE_DAG
// RUN: %HAKC_PYTHON_ADJUST_DAG
// RUN: %HAKC_PASS_COMPARTMENTALIZE
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
