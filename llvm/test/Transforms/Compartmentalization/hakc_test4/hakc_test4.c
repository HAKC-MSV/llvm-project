// RUN: %HAKC_SETUP
// RUN: %HAKC_YAML_REPLACE_PATHS
// RUN: %HAKC_YAML_CHANGE_MODE_DAG
// RUN: %HAKC_YAML_CHANGE_MODE_COMP
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_PASS
// RUN: %HAKC_EVALUATE
struct data_struct {
    int a;
};

int bar(struct data_struct *);

// dummy function named after function that is in GetNoTransferFunctions
// should work on all platforms and operating systems 
int ftrace_stub(struct data_struct *a) {
    if (a) {
        (a->a)++;
        return bar(a);
    }
    return 0;
}

// CHECK-NOT: HAKC_XFER
