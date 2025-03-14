// RUN: %HAKC_EMIT_LLVM
// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_COMP_DAG_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_RUN_DAG_PASS
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_RUN_PYTHON_DAG
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
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
// CHECK-LABEL: i32 @bar
// CHECK: store ptr @kmalloc_caches, ptr %1
// CHECK: store ptr @somevar, ptr %2
// CHECK: call i32 @foo(ptr noundef %3, ptr noundef %4),
