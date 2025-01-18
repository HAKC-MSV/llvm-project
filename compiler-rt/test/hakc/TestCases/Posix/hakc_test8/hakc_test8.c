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

struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int bar(){
    struct data_struct2 ds2; 
    foo(&ds2, kmalloc_caches, somevar);
}

int foo(struct data_struct2 *a, int* v1, int* v2) {
    if (a) {
        *v1++;
        *v2++;
        struct data_struct b;
        b.a = 0;
        return a->f(&b);
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %7 = call i8* @hakc_transfer_to_clique