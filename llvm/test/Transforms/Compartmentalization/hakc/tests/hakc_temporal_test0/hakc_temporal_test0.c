// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

void node_exit();

// Note: perform both the post dom analysis and the compartmentalization in this test
// TODO: add checks -> write post dom analysis result to text file, then use file check for result
void node_entry(int a, int b) {
  if(a > 0){
    ++a;
  }
  else{
    ++b;
  }
  node_exit();
}


// CHECK: NULL
