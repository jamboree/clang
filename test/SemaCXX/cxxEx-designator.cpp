// RUN: %clang_cc1 -std=c++11 %s -verify

void f1(int = 0); //expected-note{{previous definition is here}}
void f1(int.a);   //expected-error{{cannot be redefined}}

void f2(int);
void f2(int _);
void f2(int.a);

void f3(int);   //expected-note{{no corresponding designatable parameter}} expected-note{{declared here}}
void f4(int a); //expected-note{{parameter 'a' is not designatable}} expected-note{{declared here}}

void f5(int.a, int.b); //expected-note{{multiple arguments provided for 1st parameter}} expected-note{{declared here}}

void f6(int.a, int.b = 0); //expected-note{{missing argument for 1st parameter}} expected-note{{declared here}}

void f7(int, int.a); //expected-note{{argument index 2 is out of bounds}} expected-note{{declared here}}

void f8(void *.a, int.b); //expected-note{{no known conversion from 'int' to 'void *'}}

template<class... T>
void f9(int.a, int.b, int.c = 0, T...);

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

struct Derived : Class {
  using Class::Class;
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

void conv(Class); //expected-note{{cannot convert initializer list argument to 'Class'}} \
                  //expected-note{{cannot convert initializer list argument to 'Class'}} \
                  //expected-note{{passing argument to parameter here}} \
                  //expected-note{{passing argument to parameter here}}

void* operator new(__SIZE_TYPE__); //expected-note{{requires 1 argument, but 2 were provided}}
void *operator new(__SIZE_TYPE__.count, int.a); //expected-note{{multiple arguments provided for 1st parameter}}

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
  f9(1, .b = 2);
  f9(.b = 1, .a = 2);
  //f9(.b = 1, 2, 3, .a = 4);
  //f9(.c = 1, 2, .a = 3, 4);

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

  Agg agg1 = {.b = 1, .a = 2};

  Agg agg2 = {.a = 1,  // expected-note{{previous designator is here}}
              .a = 2}; // expected-error{{duplicate designators}}

  Agg agg3 = {1,       // expected-note{{previous initialization is here}}
              .a = 2}; // expected-error{{multiple initializations given for non-static member}}

  Class c1(.b = 1, .a = 2);
  Class c2{.b = 1, .a = 2};

  c1.f(.b = 1, .a = 2);
  auto mf = &Class::f;
  (c1.*mf)(.b = 1, .a = 2); // expected-error{{designated arguments cannot be used by member function pointer}}

  Class(.a = 1,  // expected-note{{previous designator is here}}
        .a = 2); // expected-error{{duplicate designators}}

  Class{.a = 1,  // expected-note{{previous designator is here}}
        .a = 2}; // expected-error{{duplicate designators}}

  Class(.b = 1); // expected-error{{no matching constructor for initialization}}

  Class{.a.b = 1}; // expected-error{{designator chain cannot initialize non-aggregate type}}

  Class{[0] = 1}; // expected-error{{array designator cannot initialize non-array type}}

  Derived(.a = 1).f(.b = 1, .a = 2);
  Derived{.a = 1}.f(.b = 1, .a = 2);

  using R = int&;
  int i(.x = 1);    //expected-error{{designator in initializer for scalar type}}
  R j(.x = 1);      //expected-error{{designator in initializer for reference type}}
  int(.x = 1);      //expected-error{{designator in initializer for scalar type}}
  R(.x = 1);        //expected-error{{designator in initializer for reference type}}

  MemInit{.b = 1,.a = 2};

  conv({.a = 1,.b = 2});
  conv({.a.b = 1}); //expected-error{{no matching function for call}}
  conv({[0] = 1});  //expected-error{{no matching function for call}}
  (conv)({.a = 1,.b = 2});
  (conv)({.a.b = 1}); //expected-error{{designator chain cannot initialize non-aggregate type}}
  (conv)({[0] = 1});  //expected-error{{array designator cannot initialize non-array type}}

  new(.a = 1) int;
  new(.count = 1) int; //expected-error{{no matching function for call}}

  return 0;
}