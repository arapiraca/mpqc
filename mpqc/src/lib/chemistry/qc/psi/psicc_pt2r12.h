//
// psicc_pt2r12.h
//
// Copyright (C) 2002 Edward Valeev
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

#ifdef __GNUC__
#pragma interface
#endif

#ifndef _chemistry_qc_psi_psiccpt2r12_h
#define _chemistry_qc_psi_psiccpt2r12_h

#include <chemistry/qc/mbptr12/mbptr12.h>
#include <chemistry/qc/psi/psicc.h>

namespace sc {
  
  class MBPT2_R12;
  
  ///////////////////////////////////////////////////////////////////
  /// PsiCCSD_PT2R12 is a concrete implementation of the \f${\rm CCSD}-(2)_\overline{R12}\f$ method
  class PsiCCSD_PT2R12 : public PsiCC {
      double eccsd_;

      Ref<MBPT2_R12> mbptr12_;
    protected:
      /// set to true to test the code against MP2-R12. Will also test the T2 Psi3->MPQC transform
      static const bool mp2_only_ = false;
      /// set to true to use Ts instead of Lambdas
      static const bool replace_Lambda_with_T_ = true;
      /// default is to include up to 3rd-order terms in the energy
      static const unsigned int completeness_order_for_energy_ = 3;
      /// the max order for the intermediates is one less
      static const unsigned int
          completeness_order_for_intermediates_ = completeness_order_for_energy_
              - 1;

      void write_input(int conv);
    public:
      PsiCCSD_PT2R12(const Ref<KeyVal>&);
      PsiCCSD_PT2R12(StateIn&);
      ~PsiCCSD_PT2R12();
      void save_data_state(StateOut&);
      int gradient_implemented() const;
      void compute();
      
      /// CCSD energy
      double eccsd();
  };
  
  ///////////////////////////////////////////////////////////////////
  /// PsiCCSD_PT2R12T is a concrete implementation of the \f${\rm CCSD}-(2)_\overline{R12,T}\f$ method
  class PsiCCSD_PT2R12T : public PsiCCSD_PT2R12 {
      double e_t_;
      void write_input(int conv);
    public:
      PsiCCSD_PT2R12T(const Ref<KeyVal>&);
      PsiCCSD_PT2R12T(StateIn&);
      ~PsiCCSD_PT2R12T();
      void save_data_state(StateOut&);
      int gradient_implemented() const;
      void compute();
  };

}

#endif /* header guard */