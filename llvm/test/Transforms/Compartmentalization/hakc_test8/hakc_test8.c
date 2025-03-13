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
// CHECK:
        *v1++;
        *v2++;
        struct data_struct b;
        b.a = 0;
        return a->f(&b); // this line is what causes the pass to fail 
    }
    return 0;
}

// CHECK-LABEL: HAKC_XFER_foo
