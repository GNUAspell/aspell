#ifndef __aspeller_matrix_hh__
#define __aspeller_matrix_hh__

#include <vector>

namespace aspeller {

  template <typename T>
  class Matrix {
    int x_size;
    int y_size;
    T * data;
  public:
    void init(int sx, int sy, T * d) {x_size = sx; y_size = sy; data = d;}
    Matrix() {}
    Matrix(int sx, int sy, T * d) {init(sx,sy,d);}
    T operator() (int x, int y) const {return data[x + y*x_size];}
    T & operator() (int x, int y) {return data[x + y*x_size];}
  };

}

#endif
