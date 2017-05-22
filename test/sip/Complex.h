#ifndef COMPLEX_H
#define COMPLEX_H
class Complex {
  friend bool operator ==(const Complex& a, const Complex& b);
  double real, imaginary;
public:
  Complex( double r, double i = 0 )
    : real(r)
        , imaginary(i)
  {
  }
};


#endif
