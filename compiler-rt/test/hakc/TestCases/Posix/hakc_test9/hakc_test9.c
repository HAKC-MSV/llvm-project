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

// test of nested compartment calls

struct data_struct {
    int a;
};

int bar(struct data_struct *b){
    if(b){
        return (b->a)++;
    }
    return (b->a)--; 
}

int foo(struct data_struct *a) {
    if (a) {
        struct data_struct * b;
        b->a = 0;
        return bar(b);
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %2 = call i32 @HAKC_ORIG_foo(%struct.data_struct2* %0)
// CHECK: ret i32 %2

// CHECK-LABEL: HAKC_XFER_bar
// CHECK: %2 = call i32 @HAKC_ORIG_bar(%struct.data_struct* %0)
// CHECK: ret i32 %2

