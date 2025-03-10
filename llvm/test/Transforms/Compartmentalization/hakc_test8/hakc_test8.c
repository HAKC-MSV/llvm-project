// RUN: %HAKC_PROCESS_YAML_COMP_CONFIG
// RUN: %HAKC_PROCESS_YAML_POLICY_CONFIG
// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER & sleep 1 &&\
// RUN: %HAKC_RUN_PASS
// RUN: %HAKC_EVALUATE
// test of GetIgnoredGlobals

int kmalloc_caches = 53;
int somevar = 53;

struct data_struct {
    int a;
};

struct data_struct2 {
    int (*f)(struct data_struct *);
};

int foo(struct data_struct2 *a, int* v1, int* v2) {
    if (a) {
        *v1++;
        *v2++;
        struct data_struct b;
        b.a = 0;
        return a->f(&b); // this line is what causes the pass to fail 
    }
    return 0;
}

// int bar(){
//     struct data_struct2 ds2; 
//     int *v1;
//     v1 = &kmalloc_caches; 
//     int *v2;
//     v2 = &somevar; 
//     foo(&ds2, v1, v2);
// }
// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %7 = call i8* @hakc_transfer_to_clique
