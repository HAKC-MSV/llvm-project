// RUN: %HAKC_START_POLICY_SERVER
// RUN: clang -g -S -emit-llvm -mllvm --enable-hakc -mllvm --hakc-config=%HAKC_CONFIG -o %t %s
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
