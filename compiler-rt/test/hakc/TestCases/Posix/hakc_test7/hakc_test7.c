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

// testing never used struct types 
struct list_head{
    int a;
};

int foo(struct list_head * ListHead) {
    if(ListHead){
        return 1;
    }
    return 0;
}

// note: incomplete test. need derricks help for expected behavior 
// CHECK-LABEL: HAKC_XFER_foo
// CHECK: %2 = call i32 @HAKC_ORIG_foo(%struct.list_head* %0)