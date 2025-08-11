// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

void node_setup(int a);

void node_steady_state(int a);

void node_teardown(int a);

void node_exit();

// Note: perform both the post dom analysis and the compartmentalization in this test
// TODO: add checks -> write post dom analysis result to text file, then use file check for result
void entry(int a, int b) {
  if(a > 0){
    node_setup(a);
  }
  else{
    node_setup(b);
  }
  if(b > 0){
    node_steady_state(a);
  }
  else{
    node_steady_state(b);
  }
  node_teardown(a);
  node_exit();
}


// CHECK: NULL

