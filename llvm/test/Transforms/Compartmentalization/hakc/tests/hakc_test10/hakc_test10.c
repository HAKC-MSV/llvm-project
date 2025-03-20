// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
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

int bar(){
    int *v1;
    v1 = &kmalloc_caches; 
    int *v2;
    v2 = &somevar; 
    return foo(v1, v2);
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: call ptr @hakc_transfer_to_clique(ptr %0, i64 4, i64 4, i64 13, i1 false)
// CHECK: call ptr @hakc_transfer_to_clique(ptr %1, i64 4, i64 4, i64 13, i1 false)
// CHECK: call i32 @HAKC_ORIG_foo(ptr %2, ptr %3)
