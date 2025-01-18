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
struct linked_list {
    struct linked_list *next;
};

struct data {
    struct linked_list list;
    void *data;
};

void init_data(struct data *data) {
    data->data = 0;
    data->list.next = &data->list;
}


// CHECK-LABEL: HAKC_ORIG_init_data
// CHECK: %3 = call i8* @check_hakc_data_access(i8* %2, i32 6, i64 393218)
// CHECK-LABEL: HAKC_XFER_init_data
// CHECK: %5 = call i8* @hakc_transfer_to_clique(i8* %4, i64 16, i32 6, i32 241, i1 false)
