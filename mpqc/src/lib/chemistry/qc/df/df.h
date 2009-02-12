//
// df.h
//
// Copyright (C) 2009 Edward Valeev
//
// Author: Edward Valeev <evaleev@vt.edu>
// Maintainer: EV
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

#ifdef __GNUG__
#pragma interface
#endif

#ifndef _mpqc_src_lib_chemistry_qc_df_df_h
#define _mpqc_src_lib_chemistry_qc_df_df_h

#include <util/state/state.h>
#include <util/misc/compute.h>
#include <math/scmat/result.h>
#include <chemistry/qc/basis/integral.h>

namespace sc {

  /** Represents decomposition of a product of basis sets. The definition is as follows:
      \f[
      |rs) = \sum_A C^{rs}_A |A)  \quad ,
      \f]
      where \f$ |A) \f$ represent the low-rank space. The coefficients in the expansion are returned by result().
      */
  class BasisProductDecomposition : virtual public SavableState {
    public:
      ~BasisProductDecomposition();
      BasisProductDecomposition(const Ref<Integral>& integral,
                                const Ref<GaussianBasisSet>& basis1,
                                const Ref<GaussianBasisSet>& basis2);
      BasisProductDecomposition(StateIn&);
      void save_data_state(StateOut&);

      /// column dimension corresponds to the basis product space, row dimension corresponds to the reduced rank space
      virtual const RefSCMatrix& C() const =0;

      const Ref<Integral>& integral() const { return integral_; }
      const Ref<GaussianBasisSet>& basis(unsigned int i) const { return basis_[i]; }

      virtual void compute() =0;

    protected:
      const RefSCDimension& product_dimension() const { return pdim_; }

    private:
      RefSCDimension pdim_;    //< basis product dimension

      Ref<Integral> integral_;
      Ref<GaussianBasisSet> basis_[2];

  };

  /** Decomposition by density fitting with respect to some kernel. The definition is as follows:
      \f[
      |rs) = \sum_A C^{rs}_A |A)  \quad ,
      \f]
      where \f$ |A) \f$ belong to a fitting basis and \f$ C^{rs}_A \f$ are determined by solving the linear system
      \f[
      (A|\hat{W}|rs) = \sum_B C^{rs}_B (A|\hat{W}|B)  \quad .
      \f]
      The kernel \f$\hat{W} \f$ is any definite operator (currently hardwired to Coulomb kernel).
   */
  class DensityFitting : public BasisProductDecomposition {
    public:
      ~DensityFitting();
      DensityFitting(const Ref<Integral>& integral,
                     const Ref<GaussianBasisSet>& basis1,
                     const Ref<GaussianBasisSet>& basis2,
                     const Ref<GaussianBasisSet>& fitting_basis);
      DensityFitting(StateIn&);
      void save_data_state(StateOut&);

      const RefSCMatrix& C() const { return C_; }
      /// returns the kernel represented in the fitting basis, i.e. \f$ (A|\hat{W}|B) \f$ .
      const RefSymmSCMatrix& kernel() const { return kernel_; }
      /// returns the conjugate expansion matrix, i.e. \f$ (A|\hat{W}|rs) \equiv \sum_B C^{rs}_B (A|\hat{W}|B) \f$ .
      const RefSCMatrix& conjugateC() const { return cC_; }

      // implements BasisProductDecomposition::compute()
      void compute();

    private:
      RefSCMatrix C_;
      RefSymmSCMatrix kernel_;
      RefSCMatrix cC_;

      Ref<GaussianBasisSet> fbasis_;

  };

} // end of namespace sc

#endif // end of header guard


// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
