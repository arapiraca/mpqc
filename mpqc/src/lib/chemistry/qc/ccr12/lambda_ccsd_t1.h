//
// lambda_ccsd_t1.h --- computes the L1 residual vector for Lambda-CCSD
//
// Copyright (C) 2009 Toru Shiozaki
//
// Author: Toru Shiozaki <shiozaki@qtp.ufl.edu>
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

#ifndef _chemistry_qc_ccr12_lambda_ccsd_t1_h
#define _chemistry_qc_ccr12_lambda_ccsd_t1_h

#ifdef __GNUC__
#pragma interface
#endif

#include <chemistry/qc/ccr12/ccr12_info.h>

namespace sc {

class LAMBDA_CCSD_T1 {

  protected:
   CCR12_Info* z;

   std::vector<Tensor*> in;
   std::vector<Tensor*> kn;

   void offset_k0();
   void smith_k0(); //z->t1(),z->v2()=>z->kn.at(0)
   void offset_k1();
   void smith_k1(); //z->t1(),z->l1()=>z->kn.at(1)
   void offset_k2();
   void smith_k2(); //z->t2(),z->l2()=>z->kn.at(2)
   void offset_k3();
   void smith_k3(); //z->t1(),z->l2()=>z->kn.at(3)
   void offset_k4();
   void smith_k4(); //z->t2(),z->l2()=>z->kn.at(4)
   void offset_k5();
   void smith_k5(); //z->t1(),z->v2()=>z->kn.at(5)
   void offset_k6();
   void smith_k6(); //z->t1(),z->kn.at(3)=>z->kn.at(6)
   void smith_0_1(Ref<Tensor>&); //z->f1()=>out
   void smith_0_2(Ref<Tensor>&); //z->kn.at(0)=>out
   void offset_smith_0_3();
   void smith_0_3_0(); //z->f1()=>z->in.at(1)
   void offset_smith_1_5();
   void smith_1_5_0(); //z->f1()=>z->in.at(2)
   void smith_1_5_1(); //z->kn.at(0)=>z->in.at(2)
   void smith_1_5(); //z->t1(),z->in.at(2)=>z->in.at(1)
   void smith_1_8(); //z->t1(),z->v2()=>z->in.at(1)
   void smith_1_15(); //z->t2(),z->v2()=>z->in.at(1)
   void smith_0_3(Ref<Tensor>&); //z->l1(),z->in.at(1)=>out
   void offset_smith_0_4();
   void smith_0_4_0(); //z->f1()=>z->in.at(1)
   void smith_1_9(); //z->t1(),z->v2()=>z->in.at(1)
   void smith_1_13(); //z->t1(),z->kn.at(0)=>z->in.at(1)
   void smith_0_4(Ref<Tensor>&); //z->l1(),z->in.at(1)=>out
   void offset_smith_0_6();
   void smith_0_6_0(); //z->kn.at(1)=>z->in.at(1)
   void smith_0_6_1(); //z->kn.at(2)=>z->in.at(1)
   void smith_0_6(Ref<Tensor>&); //z->f1(),z->in.at(1)=>out
   void smith_0_7(Ref<Tensor>&); //z->l1(),z->v2()=>out
   void offset_smith_0_10();
   void smith_0_10_0(); //z->kn.at(1)=>z->in.at(1)
   void smith_0_10_1(); //z->kn.at(2)=>z->in.at(1)
   void smith_0_10(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out
   void offset_smith_0_11();
   void smith_1_11(); //z->t1(),z->l1()=>z->in.at(1)
   void smith_1_35(); //z->t2(),z->l2()=>z->in.at(1)
   void smith_0_11(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out
   void offset_smith_0_14();
   void offset_smith_1_14();
   void smith_1_14_0(); //z->kn.at(1)=>z->in.at(2)
   void smith_1_14_1(); //z->kn.at(2)=>z->in.at(2)
   void smith_1_14(); //z->t1(),z->in.at(2)=>z->in.at(1)
   void smith_1_17(); //z->t2(),z->l1()=>z->in.at(1)
   void smith_1_44(); //z->t2(),z->kn.at(3)=>z->in.at(1)
   void smith_0_14(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out
   void offset_smith_0_16();
   void smith_1_16(); //z->t2(),z->l1()=>z->in.at(1)
   void offset_smith_1_37();
   void smith_1_37_0(); //z->kn.at(6)=>z->in.at(2)
   void smith_1_37_1(); //z->kn.at(4)=>z->in.at(2)
   void smith_1_37(); //z->t1(),z->in.at(2)=>z->in.at(1)
   void smith_1_41(); //z->t2(),z->kn.at(3)=>z->in.at(1)
   void smith_1_43(); //z->t1(),z->kn.at(2)=>z->in.at(1)
   void smith_0_16(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out
   void offset_smith_0_18();
   void smith_0_18_0(); //z->v2()=>z->in.at(1)
   void offset_smith_1_18();
   void smith_1_18_0(); //z->f1()=>z->in.at(2)
   void smith_1_18_1(); //z->kn.at(0)=>z->in.at(2)
   void smith_1_18(); //z->t2(),z->in.at(2)=>z->in.at(1)
   void offset_smith_1_23();
   void smith_1_23_0(); //z->v2()=>z->in.at(2)
   void smith_2_27(); //z->t1(),z->v2()=>z->in.at(2)
   void smith_1_23(); //z->t1(),z->in.at(2)=>z->in.at(1)
   void offset_smith_1_26();
   void offset_smith_2_26();
   void smith_2_26_0(); //z->v2()=>z->in.at(3)
   void smith_2_26_1(); //z->kn.at(5)=>z->in.at(3)
   void smith_2_26(); //z->t1(),z->in.at(3)=>z->in.at(2)
   void smith_2_38(); //z->t2(),z->v2()=>z->in.at(2)
   void smith_1_26(); //z->t1(),z->in.at(2)=>z->in.at(1)
   void offset_smith_1_30();
   void smith_1_30_0(); //z->v2()=>z->in.at(2)
   void smith_1_30_1(); //z->kn.at(5)=>z->in.at(2)
   void smith_1_30(); //z->t2(),z->in.at(2)=>z->in.at(1)
   void smith_1_31(); //z->t2(),z->v2()=>z->in.at(1)
   void smith_0_18(Ref<Tensor>&); //z->l2(),z->in.at(1)=>out
   void smith_0_21(Ref<Tensor>&); //z->l2(),z->v2()=>out
   void smith_0_22(Ref<Tensor>&); //z->v2(),z->kn.at(3)=>out
   void smith_0_24(Ref<Tensor>&); //z->v2(),z->kn.at(3)=>out
   void offset_smith_0_25();
   void smith_1_25(); //z->t1(),z->l2()=>z->in.at(1)
   void smith_0_25(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out
   void offset_smith_0_28();
   void smith_0_28_0(); //z->kn.at(6)=>z->in.at(1)
   void smith_0_28_1(); //z->kn.at(4)=>z->in.at(1)
   void smith_0_28(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out
   void offset_smith_0_29();
   void smith_1_29(); //z->t1(),z->kn.at(3)=>z->in.at(1)
   void smith_1_33(); //z->t2(),z->l2()=>z->in.at(1)
   void smith_0_29(Ref<Tensor>&); //z->v2(),z->in.at(1)=>out


  public:
   LAMBDA_CCSD_T1(CCR12_Info* info);
   ~LAMBDA_CCSD_T1();
    
   void compute_amp(Ref<Tensor>& out);

};



}

#endif


