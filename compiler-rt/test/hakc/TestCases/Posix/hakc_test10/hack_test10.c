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

// test of GetIgnoredGlobals

int* kmalloc_caches = 53;
int* somevar = 53;

int bar(){
    return foo(kmalloc_caches, somevar);
}

int foo(int* v1, int* v2) {
    if (&v1) {
        *v2++;
        return *v2;
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %3 = bitcast i32* %1 to i8*, !dbg !39
// CHECK: %4 = call i32 @get_hakc_address_color(i8* %3), !dbg !39
// CHECK: %5 = bitcast i32* %1 to i8*, !dbg !39
// CHECK: %6 = call i8* @hakc_transfer_to_clique(i8* %5, i64 4, i32 6, i32 241, i1 false), !dbg !39
// CHECK: %7 = bitcast i8* %6 to i32*, !dbg !39
// CHECK: %8 = call i32 @HAKC_ORIG_foo(i32* %0, i32* %7), !dbg !39
// CHECK: %9 = bitcast i32* %1 to i8*, !dbg !39
// CHECK: call void @hakc_color_address(i8* %9, i32 %4, i64 4), !dbg !39
// CHECK: ret i32 %8, !dbg !39