//=============================================================================
// FILE:
//      input_for_cc.c
//
// DESCRIPTION:
//      Sample input file for CallCounter analysis.
//
// License: MIT
//=============================================================================

int counter_foo = 0;
int counter_bar = 0;
int counter_fez = 0;
int counter_main = 0;

void foo() {
    ++counter_foo;
}
void bar() {
    ++counter_bar;
    foo();
}
void fez() {
    ++counter_fez;
    bar();
}

int main() {
  ++counter_main;
  foo();
  bar();
  fez();

  int ii = 0;
  for (ii = 0; ii < 10; ii++)
    foo();

  return 0;
}

int printf(char *str, ...);

void printf_wrapper() { }
