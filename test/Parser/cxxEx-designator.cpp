// RUN: %clang_cc1 %s -verify

void f1(int.); //expected-error{{expected unqualified-id}}

void f3(int.a);

int main() {
  f3(.); //expected-error{{expected a field designator}}
  (int)(.x = 1); //expected-error{{expected expression}}
  return 0;
}