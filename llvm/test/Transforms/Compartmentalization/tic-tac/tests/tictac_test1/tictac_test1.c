// RUN: %HAKC_PYTHON_VENV
// RUN: %HAKC_START_POLICY_SERVER
// RUN: %HAKC_RUN_COMP_PASS
// RUN: %HAKC_EVALUATE

void setup(int a);

void steady_state(int a);

void teardown(int a);

void exit();

// Note: perform both the post dom analysis and the compartmentalization in this test
// TODO: add checks -> write post dom analysis result to text file, then use file check for result
void entry(int a, int b) {
  setup(a);
  steady_state(a);
  teardown(a);
  exit();
}
