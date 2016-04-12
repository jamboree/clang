// RUN: %clang_cc1 -std=c++11 %s -verify

void f1(int = 0); //expected-note{{previous definition is here}}
void f1(int.a);   //expected-error{{cannot be redefined}}

void f2(int.a);

void f3(int);   //expected-note{{no corresponding designatable parameter}} expected-note{{declared here}}
void f4(int a); //expected-note{{parameter 'a' is not designatable}} expected-note{{declared here}}

void f5(int.a, int.b); //expected-note{{multiple arguments provided for 1st parameter}} expected-note{{declared here}}

void f6(int.a, int.b = 0); //expected-note{{missing argument for 1st parameter}} expected-note{{declared here}}

void f7(int, int.a); //expected-note{{argument index 2 is out of bounds}} expected-note{{declared here}}

void f8(void *.a, int.b); //expected-note{{no known conversion from 'int' to 'void *'}}

template <class F>
auto fn(F f) -> decltype(f(.a = 1,   //expected-note{{previous designator is here}}
                           .a = 2)); //expected-error{{duplicate designators}}

struct Agg {
  int a;
  int b;
};

struct Class { //expected-note{{candidate constructor (the implicit copy constructor) not viable}} \
               //expected-note{{candidate constructor (the implicit move constructor) not viable}}
  Class(int.a, int.b = 0); //expected-note{{missing argument for 1st parameter}}
  void f(int.a, int.b);
};

using FP = void (*)(int);
struct Surrogate {
  operator FP() const { return f2; } //expected-note{{designated arguments cannot be used by surrogate}}
};

struct MemInit : Agg, Class {
  Agg agg;
  Class c1, c2;

  MemInit(int.a, int.b)
      : Agg{.a = a}, Class(.a = a), agg{.b = b}, c1{.a = a}, c2(.b = b, .a = a) {}
};

int main() {
  f2(.a = 1);
  (f2)(.a = 1);
  f5(.b = 1, .a = 2);
  (f5)(.b = 1, .a = 2);
  f6(.a = 1);
  (f6)(.a = 1);
  f7(0, .a = 1);
  (f7)(0, .a = 1);
  f8(.b = 1, .a = nullptr);
  (f8)(.b = 1, .a = nullptr);

  f2(.a = 1,            //expected-note{{previous designator is here}}
     .a = 2);           //expected-error{{duplicate designators}}
  f3(.a = 1);           //expected-error{{no matching function}}
  f4(.a = 1);           //expected-error{{no matching function}}
  f5(1, .a = 2);        //expected-error{{no matching function}}
  f6(.b = 2);           //expected-error{{no matching function}}
  f7(.a = 1, 2);        //expected-error{{no matching function}}
  f8(.b = 1, .a = 2);   //expected-error{{no matching function}}
  Surrogate s;
  s(.a = 1);            //expected-error{{no matching function}}

  FP fp = f2;
  fp(.a = 1);       //expected-error{{designated arguments cannot be used by function pointer}}
  (&f2)(.a = 1);    //expected-error{{designated arguments cannot be used by function pointer}}
  (f3)(.a = 1);     //expected-error{{no corresponding designatable parameter}}
  (f4)(.a = 1);     //expected-error{{parameter 'a' is not designatable}}
  (f5)(1, .a = 2);  //expected-error{{multiple arguments provided for 1st parameter}}
  (f6)(.b = 2);     //expected-error{{missing argument for 1st parameter}}
  (f7)(.a = 1, 2);  //expected-error{{argument index 2 is out of bounds}}

  Agg agg = {.b = 1,.a = 2};

  Agg agg1 = {.a = 1,    //expected-note{{previous designator is here}}
              .a = 2};   //expected-error{{duplicate designators}}

  Agg agg2 = {1,       //expected-note{{previous initialization is here}}
              .a = 2}; //expected-error{{multiple initializations given for non-static member}}

  Class c1(.b = 1, .a = 2);
  Class c2{.b = 1, .a = 2};

  c1.f(.b = 1, .a = 2);
  auto mf = &Class::f;
  (c1.*mf)(.b = 1, .a = 2); //expected-error{{designated arguments cannot be used by member function pointer}}

  Class c3(.a = 1,  //expected-note{{previous designator is here}}
           .a = 2); //expected-error{{duplicate designators}}

  Class c4{.a = 1,  //expected-note{{previous designator is here}}
           .a = 2}; //expected-error{{duplicate designators}}

  Class c5(.b = 1); //expected-error{{no matching constructor for initialization}}

  using R = int&;
  int i(.x = 1);    //expected-error{{designator in initializer for scalar type}}
  R j(.x = 1);      //expected-error{{designator in initializer for reference type}}
  int(.x = 1);      //expected-error{{designator in initializer for scalar type}}
  R(.x = 1);        //expected-error{{designator in initializer for reference type}}

  MemInit{.b = 1,.a = 2};

  return 0;
}