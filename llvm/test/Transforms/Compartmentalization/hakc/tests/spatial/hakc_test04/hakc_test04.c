// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_ANALYSIS_SERVER
// RUN: %HAKC_RUN_ANALYSIS_PASS
// RUN: %HAKC_START_ENFORCEMENT_SERVER
// RUN: %HAKC_RUN_ENFORCEMENT_PASS
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

// NOTE: fails, since get no transfer functions is not implemented
// CHECK-NOT: @HAKC_XFER_ftrace_stub(ptr noundef %0)
