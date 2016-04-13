// RUN: %clang_cc1 %s -verify

void f1(int.); //expected-error{{expected unqualified-id}}

void f3(int.a);

void f4(void(*.a)(), char.b[], char(&.c)[4]);

int main() {
  f3(.); //expected-error{{expected a field designator}}
  f3(.a);       //expected-error{{expected '=' after designator}}
  f3(.a.b = 1); //expected-error{{expected '=' after designator}}
  (int)(.x = 1); //expected-error{{expected expression}}
  return 0;
}