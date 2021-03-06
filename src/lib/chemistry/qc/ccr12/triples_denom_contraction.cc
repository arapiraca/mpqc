//
// triples_denom_contraction.cc
//
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki.toru@gmail.com>
// Maintainer: TS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//


#include <sstream>
#include <algorithm>
#include <chemistry/qc/ccr12/ccr12_triples.h>
#include <math/scmat/blas.h>
#include <chemistry/qc/ccr12/tensor.h>
#include <chemistry/qc/ccr12/mtensor.h>

using namespace sc;
using namespace std;


void CCR12_Triples::denom_contraction_ig(){

  const size_t maxtile = z->maxtilesize();
  const size_t singles = maxtile * maxtile;
  const size_t doubles = singles * singles;
  const size_t triples = singles * doubles;
  double* k_c      = z->mem()->malloc_local_double(triples);
  double* k_c_sort = z->mem()->malloc_local_double(singles);
  double* k_a0     = z->mem()->malloc_local_double(doubles);
  double* k_a1     = z->mem()->malloc_local_double(triples);

  const long noab = z->noab();
  const long nvab = z->nvab();

  const int nocc_act = z->naoa();
  const int psize = z->naoa() + z->nava();
  const int target_pairsize = z->naoa() * (z->naoa() + z->nava());
  const int oosize = z->naoa() * z->naoa();
  const int oasize = z->naoa() * z->nava();

#if 0
  MTensor<4>::tile_ranges iiii(4, MTensor<4>::tile_range(0, noab+nvab));
  MTensor<4>::element_ranges iiii_erange(4, MTensor<4>::element_range(0, nocc_act) );
  vector<long> amap;
  {
    vector<int> intmap = sc::map(*(z->r12eval()->occ_act(Alpha)), *z->corr_space(), false);
    amap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), amap.begin());
  }

  Ref<SCMatrixKit> kit = SCMatrixKit::default_matrixkit();
  RefSCDimension dim_pair(new SCDimension(target_pairsize));
  RefSymmSCMatrix refinverse_ipip = kit->symmmatrix(dim_pair);
  RefSCMatrix refinverse_ippi = kit->matrix(dim_pair, dim_pair);
#endif

  // the loops are reordered to minimize the inversion...
  int count = 0;
  for (long p6b = noab; p6b < noab+nvab; ++p6b) {
   for (long h1b = 0L; h1b < noab; ++h1b) {
    for (long h2b = h1b; h2b < noab; ++h2b) {
     for (long h3b = h2b; h3b < noab; ++h3b) {
      const int rh1b = z->get_range(h1b);
      const int rh2b = z->get_range(h2b);
      const int rh3b = z->get_range(h3b);
      const int rp6b = z->get_range(p6b);
      for (int p6 = 0; p6 < rp6b; ++p6) {
       for (int h1 = 0; h1 < rh1b; ++h1) {
        for (int h2 = 0; h2 < rh2b; ++h2) {
         for (int h3 = 0; h3 < rh3b; ++h3, ++count) {
          if (z->get_offset(h1b) + h1 == z->get_offset(h2b) + h2) continue;
          if (z->get_offset(h1b) + h1 == z->get_offset(h3b) + h3) continue;
          if (z->get_offset(h3b) + h3 == z->get_offset(h2b) + h2) continue;

          if (count % z->mem()->n() != z->mem()->me() ) continue;

          const double ep6 = z->get_orb_energy(z->get_offset(p6b) + p6);
          const double eh1 = z->get_orb_energy(z->get_offset(h1b) + h1);
          const double eh2 = z->get_orb_energy(z->get_offset(h2b) + h2);
          const double eh3 = z->get_orb_energy(z->get_offset(h3b) + h3);

          // current denominator tensor
          stringstream ss;
          ss << "denom" << count;
          Ref<Tensor> denom = new Tensor(ss.str(), z->mem());
          z->offset_x_gen(denom, false, false);

          RefSymmSCMatrix refxminusb_ip = z->X_ip() * (eh1 + eh2 + eh3 - ep6) - z->B_ip();
          // this line takes forever.
          RefSymmSCMatrix refinverse_ip = refxminusb_ip.gi();

          /* X_ip matrix is organized as follows:
             __ii___ia___ai__
          ii |    |    |    |
             |____|____|____|
          ia |    |    |    |
             |____|____|____|
          ai |    |    |    |
             |____|____|____|

          */

#if 0
          MTensor<4> D(z, denom.pointer(), iiii);
          D.convert(refinverse_ipip, refinverse_ippi, nocc_act, nocc_act, false, false,
                    amap, amap, amap, amap, &iiii_erange);
#endif

          const size_t h3216 = h3 + rh3b * (h2 + rh2b * (h1 + rh1b * p6));
          const blasint stride = rh3b * rh2b * rh1b * rp6b;

          for (long h4b = 0L; h4b < noab; ++h4b) {
           for (long g5b = h4b; g5b < noab+nvab; ++g5b) {

            const int rh4b = z->get_range(h4b);
            const int rg5b = z->get_range(g5b);

            // spin & spatial symmetries...
            if (!z->restricted() ||
                z->get_spin(h4b)+z->get_spin(g5b)+z->get_spin(p6b)+z->get_spin(h1b)+z->get_spin(h2b)+z->get_spin(h3b) != 12L) {
             if (z->get_spin(h4b)+z->get_spin(g5b)+z->get_spin(p6b) == z->get_spin(h1b)+z->get_spin(h2b)+z->get_spin(h3b)) {
              if ((z->get_sym(h4b)^(z->get_sym(g5b)^(z->get_sym(p6b)^(z->get_sym(h1b)^(z->get_sym(h2b)^z->get_sym(h3b))))))
                  == z->irrep_t()) {

               std::fill(k_c_sort, k_c_sort+rh4b*rg5b, 0.0);

               // summation loops
               for (long h7b = 0L; h7b < noab; ++h7b) {
                for (long g8b = h7b; g8b < noab+nvab; ++g8b) {
                 const int rh7b = z->get_range(h7b);
                 const int rg8b = z->get_range(g8b);

                 if (z->get_spin(h4b)+z->get_spin(g5b) == z->get_spin(h7b)+z->get_spin(g8b)) {
                  if ((z->get_sym(h4b)^(z->get_sym(g5b)^(z->get_sym(h7b)^z->get_sym(g8b)))) == z->irrep_t()) {

                   long h4b_0, g5b_0, h7b_0, g8b_0;
                   z->restricted_4(h4b, g5b, h7b, g8b,
                                   h4b_0, g5b_0, h7b_0, g8b_0);

                   long h7b_1, g8b_1, p6b_1, h1b_1, h2b_1, h3b_1;
                   z->restricted_6(h7b, g8b, p6b, h1b, h2b, h3b,
                                   h7b_1, g8b_1, p6b_1, h1b_1, h2b_1, h3b_1);

                   const blasint dim_common = rh7b * rg8b;
                   const blasint dima0_sort = rh4b * rg5b;

                   // read the "denominator" tensor
                   denom->get_block(g8b_0+(noab+nvab)*(h7b_0+(noab+nvab)*(g5b_0+(noab+nvab)*h4b_0)), k_a0);

                   // read the "doubles" intermediate (right-hand-side numerator)
                   doubles_intermediate_->get_block(h3b_1+noab*(h2b_1+noab*(h1b_1+noab
                                                  *(p6b_1-noab+nvab*(g8b_1+(noab+nvab)*h7b_1)))), k_a1);

                   double factor = h7b == g8b ? 0.5 : 1.0;
                   const blasint unit = 1;
                   const double one = 1.0;
                   F77_DGEMV("t", &dim_common, &dima0_sort,
                             &factor, k_a0, &dim_common,
                             k_a1 + h3216, &stride,
                             &one, k_c_sort, &unit);
                  }
                 }
                }
               }
               const size_t dimc = rh4b*rg5b*rp6b*rh1b*rh2b*rh3b;
               std::fill(k_c, k_c+dimc, 0.0);
               {
                size_t iall = h3216;
                size_t i = 0;
                for (int h4 = 0; h4 != rh4b; ++h4) {
                 for (int g5 = 0; g5 != rg5b; ++g5, iall += stride, ++i) {
                   k_c[iall] = k_c_sort[i];
                 }
                }
               }
               rhs_intermediate_->add_block(h3b+noab*(h2b+noab*(h1b+noab
                                      *(p6b-noab+nvab*(g5b+(noab+nvab)*h4b)))), k_c);
              }
             }
            }
           }
          }
         }
        }
       }
      }
     }
    }
   }
  }
  z->mem()->free_local_double(k_c);
  z->mem()->free_local_double(k_c_sort);
  z->mem()->free_local_double(k_a0);
  z->mem()->free_local_double(k_a1);
  z->mem()->sync();
}



void CCR12_Triples::denom_contraction(){

  const size_t maxtile = z->maxtilesize();
  const size_t singles = maxtile * maxtile;
  const size_t doubles = singles * singles;
  const size_t triples = singles * doubles;
  double* k_c      = z->mem()->malloc_local_double(triples);
  double* k_c_sort = z->mem()->malloc_local_double(singles);
  double* k_a0     = z->mem()->malloc_local_double(doubles);
  double* k_a1     = z->mem()->malloc_local_double(triples);

  const long noab = z->noab();
  const long nvab = z->nvab();

  const int nocc_act = z->naoa();
  MTensor<4>::tile_ranges iiii(4, MTensor<4>::tile_range(0, noab));
  MTensor<4>::element_ranges iiii_erange(4, MTensor<4>::element_range(0, nocc_act) );
  vector<long> amap;
  {
    vector<int> intmap = sc::map(*(z->r12eval()->occ_act(Alpha)), *z->corr_space(), false);
    amap.resize(intmap.size());
    std::copy(intmap.begin(), intmap.end(), amap.begin());
  }

  // the loops are reordered to minimize the inversion...
  int count = 0;
  for (long p6b = noab; p6b < noab+nvab; ++p6b) {
   for (long h1b = 0L; h1b < noab; ++h1b) {
    for (long h2b = h1b; h2b < noab; ++h2b) {
     for (long h3b = h2b; h3b < noab; ++h3b) {
      const int rh1b = z->get_range(h1b);
      const int rh2b = z->get_range(h2b);
      const int rh3b = z->get_range(h3b);
      const int rp6b = z->get_range(p6b);
      for (int p6 = 0; p6 < rp6b; ++p6) {
       for (int h1 = 0; h1 < rh1b; ++h1) {
        for (int h2 = 0; h2 < rh2b; ++h2) {
         for (int h3 = 0; h3 < rh3b; ++h3, ++count) {
          if (z->get_offset(h1b) + h1 == z->get_offset(h2b) + h2) continue;
          if (z->get_offset(h1b) + h1 == z->get_offset(h3b) + h3) continue;
          if (z->get_offset(h3b) + h3 == z->get_offset(h2b) + h2) continue;

          if (count % z->mem()->n() != z->mem()->me() ) continue;

          const double ep6 = z->get_orb_energy(z->get_offset(p6b) + p6);
          const double eh1 = z->get_orb_energy(z->get_offset(h1b) + h1);
          const double eh2 = z->get_orb_energy(z->get_offset(h2b) + h2);
          const double eh3 = z->get_orb_energy(z->get_offset(h3b) + h3);

          // current denominator tensor
          stringstream ss;
          ss << "denom" << count;
          Ref<Tensor> denom = new Tensor(ss.str(), z->mem());
          z->offset_gt2(denom, false);
          MTensor<4> D(z, denom.pointer(), iiii);

          RefSymmSCMatrix refxminusb = z->X() * (eh1 + eh2 + eh3 - ep6) - z->B();
          RefSymmSCMatrix refinverse = refxminusb.gi();

          D.convert(refinverse, nocc_act, nocc_act,
                    amap, amap, amap, amap, &iiii_erange);

          const size_t h3216 = h3 + rh3b * (h2 + rh2b * (h1 + rh1b * p6));
          const blasint stride = rh3b * rh2b * rh1b * rp6b;

          for (long h4b = 0L; h4b < noab; ++h4b) {
           for (long h5b = h4b; h5b < noab; ++h5b) {

            const int rh4b = z->get_range(h4b);
            const int rh5b = z->get_range(h5b);

            // spin & spatial symmetries...
            if (!z->restricted() ||
                z->get_spin(h4b)+z->get_spin(h5b)+z->get_spin(p6b)+z->get_spin(h1b)+z->get_spin(h2b)+z->get_spin(h3b) != 12L) {
             if (z->get_spin(h4b)+z->get_spin(h5b)+z->get_spin(p6b) == z->get_spin(h1b)+z->get_spin(h2b)+z->get_spin(h3b)) {
              if ((z->get_sym(h4b)^(z->get_sym(h5b)^(z->get_sym(p6b)^(z->get_sym(h1b)^(z->get_sym(h2b)^z->get_sym(h3b))))))
                  == z->irrep_t()) {

               std::fill(k_c_sort, k_c_sort+rh4b*rh5b, 0.0);

               // summation loops
               for (long h7b = 0L; h7b < noab; ++h7b) {
                for (long h8b = h7b; h8b < noab; ++h8b) {
                 const int rh7b = z->get_range(h7b);
                 const int rh8b = z->get_range(h8b);

                 if (z->get_spin(h4b)+z->get_spin(h5b) == z->get_spin(h7b)+z->get_spin(h8b)) {
                  if ((z->get_sym(h4b)^(z->get_sym(h5b)^(z->get_sym(h7b)^z->get_sym(h8b)))) == z->irrep_t()) {

                   long h4b_0, h5b_0, h7b_0, h8b_0;
                   z->restricted_4(h4b, h5b, h7b, h8b,
                                   h4b_0, h5b_0, h7b_0, h8b_0);

                   long h7b_1, h8b_1, p6b_1, h1b_1, h2b_1, h3b_1;
                   z->restricted_6(h7b, h8b, p6b, h1b, h2b, h3b,
                                   h7b_1, h8b_1, p6b_1, h1b_1, h2b_1, h3b_1);

                   const blasint dim_common = rh7b * rh8b;
                   const blasint dima0_sort = rh4b * rh5b;

                   // read the "denominator" tensor
                   denom->get_block(h8b_0+noab*(h7b_0+noab*(h5b_0+noab*h4b_0)), k_a0);

                   // read the "doubles" intermediate (right-hand-side numerator)
                   doubles_intermediate_->get_block(h3b_1+noab*(h2b_1+noab*(h1b_1+noab
                                                  *(p6b_1-noab+nvab*(h8b_1+noab*h7b_1)))), k_a1);

                   double factor = 1.0;
                   if (h7b == h8b) factor *= 0.5;
                   const blasint unit = 1;                   const double one = 1.0;
                   F77_DGEMV("t", &dim_common, &dima0_sort,
                             &factor, k_a0, &dim_common,
                             k_a1 + h3216, &stride,
                             &one, k_c_sort, &unit);
                  }
                 }
                }
               }
               const size_t dimc = rh4b*rh5b*rp6b*rh1b*rh2b*rh3b;
               std::fill(k_c, k_c+dimc, 0.0);
               {
                size_t iall = h3216;
                size_t i = 0;
                for (int h4 = 0; h4 != rh4b; ++h4) {
                 for (int h5 = 0; h5 != rh5b; ++h5, iall += stride, ++i) {
                   k_c[iall] = k_c_sort[i];
                 }
                }
               }
               rhs_intermediate_->add_block(h3b+noab*(h2b+noab*(h1b+noab
                                      *(p6b-noab+nvab*(h5b+noab*h4b)))), k_c);
              }
             }
            }
           }
          }
         }
        }
       }
      }
     }
    }
   }
  }
  z->mem()->free_local_double(k_c);
  z->mem()->free_local_double(k_c_sort);
  z->mem()->free_local_double(k_a0);
  z->mem()->free_local_double(k_a1);
  z->mem()->sync();
}

