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

// CHECK-NOT: @HAKC_XFER_ftrace_stub(ptr noundef %0)
