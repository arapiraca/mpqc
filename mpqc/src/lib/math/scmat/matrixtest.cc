
#include <iostream.h>
#include <math.h>
#include <matrix.h>

void
randomize(RefSCMatrix&m)
{
  for (int i=0; i<m.rowdim().n(); i++) {
      for (int j=0; j<m.coldim().n(); j++) {
          m.set_element(i,j,drand48());
        }
    }
}

void
randomize(RefSymmSCMatrix&m)
{
  for (int i=0; i<m.dim().n(); i++) {
      for (int j=0; j<=i; j++) {
          m.set_element(i,j,drand48());
        }
    }
}

void
randomize(RefSCVector&m)
{
  for (int i=0; i<m.dim().n(); i++) {
      m.set_element(i,drand48());
    }
}

// test abstract matrices
void
matrixtest(RefSCDimension d1,RefSCDimension d2,RefSCDimension d3)
{
  int i;
  int j;

  // seed the random number generator
  srand48(0);

  RefSCMatrix a(d1,d2);
  RefSCMatrix b(d2,d3);
  RefSCMatrix c(d1,d3);

  cout << "a(" << a.nrow() << "," << a.ncol() << ")\n";

  a.assign(0.0);
  b.assign(1.0);
  c.assign(2.0);

  a.print("a");
  b.print("b");
  c.print("c");

  RefSCMatrix d = c * b.t();

  d.print("d");

  RefSymmSCMatrix e(d3);

  e.assign(1.0);
  e.eigvals().print("e.eigvals()");
  e.eigvecs().print("e.eigvecs()");

  RefSymmSCMatrix f(d3);
  for (i=0; i<d3.n(); i++) {
      for (j=0; j<=i; j++) {
          f(i,j) = i + sqrt((double)j);
        }
    }
  f.print("f");
  f.eigvals().print("f.eigvals()");
  f.i().print("f.i()");

  RefSCMatrix g(d3,d3);
  for (i=0; i<d3.n(); i++) {
      for (j=0; j<d3.n(); j++) {
          if (i>j) g(i,j) = i + sqrt((double)j);
          else g(i,j) = j + sqrt((double)i);
        }
    }
  g.print("g");
  g.i().print("g.i()");
  (g * g.i()).print("g * g.i()");

  RefSCVector v(d3);
  for (i=0; i<d3.n(); i++) {
      v(i) = 1.0/(i+1);
    }
  v.print("Vector v");

  RefSCVector wa(d3);
  RefSCMatrix ma(d1,d3);
  randomize(ma);
  randomize(wa);
  RefSCVector va = ma * wa;
  ma.print("Matrix ma");
  va.print("Vector va");
  wa.print("Vector wa");

  RefSCVector wb(d1);
  RefSCMatrix mb(d3,d1);
  randomize(mb);
  randomize(wb);
  RefSCVector vb = mb * wb;
  ma.print("Matrix mb");
  va.print("Vector vb");
  wa.print("Vector wb");

  RefSymmSCMatrix bmbt(d3);
  RefSCMatrix redundant_ortho(d2,d3);
  RefSymmSCMatrix bmbt_fixed(d2);
  RefSCMatrix bmbt_fix_red(d2,d3);
  bmbt.assign(0.0);
  randomize(redundant_ortho);
  randomize(bmbt_fixed);
  randomize(bmbt_fix_red);
  bmbt.accumulate_transform(redundant_ortho.t(), bmbt_fixed);
  bmbt.accumulate_symmetric_sum(redundant_ortho.t() * bmbt_fix_red);
  cout << "bmbt:\n";
  bmbt.print();

  RefSCMatrix bmbt_test;
  RefSCMatrix bmbt_fixed_test(d2,d2);
  for (i=0; i<d2.n(); i++) {
      for (j=0; j<=i; j++) {
          bmbt_fixed_test(i,j) = bmbt_fixed(i,j);
          bmbt_fixed_test(j,i) = bmbt_fixed(i,j);
        }
    }
  RefSCMatrix tmp = redundant_ortho.t() * bmbt_fix_red;
  bmbt_test =  redundant_ortho.t() * bmbt_fixed_test * redundant_ortho
             + tmp + tmp.t();
  cout << "bmbt_test\n";
  bmbt_test.print();

}
