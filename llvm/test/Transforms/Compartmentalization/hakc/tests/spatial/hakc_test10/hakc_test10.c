// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_ANALYSIS_SERVER
// RUN: %HAKC_RUN_ANALYSIS_PASS
// RUN: %HAKC_START_ENFORCEMENT_SERVER
// RUN: %HAKC_RUN_ENFORCEMENT_PASS
// RUN: %HAKC_EVALUATE

// test of GetIgnoredGlobals

int kmalloc_caches = 53;
int somevar = 53;

int foo(int* v1, int* v2) {
    if (&v1) {
        *v2++;
        return *v2;
    }
    return 0;
}

// CHECK-LABEL: @HAKC_XFER_foo

int bar(){
// CHECK-LABEL: @bar
    int *v1;
    v1 = &kmalloc_caches;
    *v1 = 1;
    int *v2;
    v2 = &somevar;
// CHECK: call i32 @HAKC_XFER_foo
    return foo(v1, v2);
}
