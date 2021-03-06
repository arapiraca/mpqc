//
// compute_VXB_a_vbsneqobs.cc
//
// Copyright (C) 2005 Edward Valeev
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

#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/creator.h>
#include <chemistry/qc/mbptr12/container.h>
#include <chemistry/qc/mbptr12/compute_tbint_tensor.h>
#include <chemistry/qc/mbptr12/contract_tbint_tensor.h>

using namespace std;
using namespace sc;


/**
   R12IntEval::contrib_to_VXB_a_() computes V, X, and B intermediates in standard approximation A using tensor contract functions
   OBS != VBS assumed
*/
void
R12IntEval::contrib_to_VXB_a_vbsneqobs_()
{
  if (evaluated_)
    return;

  const bool obs_eq_vbs = r12world()->obs_eq_vbs();
  const bool obs_eq_ribs = r12world()->obs_eq_ribs();
  // commutators only appear in A', A'', and B
  const bool compute_B = (stdapprox() == R12Technology::StdApprox_Ap ||
      stdapprox() == R12Technology::StdApprox_App || stdapprox() == R12Technology::StdApprox_B);

  if (obs_eq_vbs)
      throw ProgrammingError("R12IntEval::contrib_to_VXB_a_vbsneqobs_() -- can't use this builder if OBS == VBS",__FILE__,__LINE__);

  const R12Technology::ABSMethod absmethod = r12world()->r12tech()->abs_method();

  Timer tim("mp2-f12a intermeds (tensor contract)");

  // Test new tensor compute function
  for(int s=0; s<nspincases2(); s++) {
      using namespace sc::mbptr12;
      const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
      const SpinCase1 spin1 = case1(spincase2);
      const SpinCase1 spin2 = case2(spincase2);

      const Ref<OrbitalSpace>& occ1_act = occ_act(spin1);
      const Ref<OrbitalSpace>& occ2_act = occ_act(spin2);
      const Ref<OrbitalSpace>& occ1 = occ(spin1);
      const Ref<OrbitalSpace>& occ2 = occ(spin2);
      const Ref<OrbitalSpace>& vir1_act = vir_act(spin1);
      const Ref<OrbitalSpace>& vir2_act = vir_act(spin2);
      const Ref<OrbitalSpace>& GGspace1 = GGspace(spin1);
      const Ref<OrbitalSpace>& GGspace2 = GGspace(spin2);
      const Ref<OrbitalSpace>& cabs1 = r12world()->cabs_space(spin1);
      const Ref<OrbitalSpace>& cabs2 = r12world()->cabs_space(spin2);

      // for now geminal-generating products must have same equivalence as the occupied orbitals
      const bool occ1_eq_occ2 = (occ1_act == occ2_act);
      const bool x1_eq_x2 = (GGspace1 == GGspace2);
      if (occ1_eq_occ2 ^ x1_eq_x2) {
	  throw ProgrammingError("R12IntEval::contrib_to_VXB_a_vbsneqobs_() -- this orbital_product cannot be handled yet",__FILE__,__LINE__);
      }

      // are particles 1 and 2 equivalent?
      const bool part1_equiv_part2 =  spincase2 != AlphaBeta || occ1_eq_occ2;
      // Need to antisymmetrize 1 and 2
      const bool antisymmetrize = spincase2 != AlphaBeta;

      // some transforms can be skipped if occ1/occ2 is a subset of x1/x2
      // for now it's always true since can only use ij and pq products to generate geminals
      const bool occ12_in_x12 = true;

      const double asymm_contr_pfac = part1_equiv_part2 ? -2.0 : -1.0;
      // (im|jn) contribution
      {
	  Ref<OrbitalSpace> cs1 = occ1;
	  Ref<OrbitalSpace> cs2 = occ2;
	  Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(cs1->rank(),cs2->rank(),-1.0);
	  std::vector<std::string> tforms_f12;
	  {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),GGspace1,cs1,GGspace2,cs2,
	                                               corrfactor(),true);
	      fill_container(tformkey_creator,tforms_f12);
	  }
	  std::vector<std::string> tforms;
	  if (!occ12_in_x12) {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),occ1_act,cs1,occ2_act,cs2,
                                                   corrfactor());
	      fill_container(tformkey_creator,tforms);
	  }
	  else
	      tforms.push_back(tforms_f12[0]);

      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,false,false>
          (V_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_eri(),
	   GGspace1, GGspace2, cs1, cs2,
	   occ1_act, occ2_act, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (X_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      if (compute_B) {
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      B_[s].scale(0.5); RefSCMatrix Bt = B_[s].t(); B_[s].accumulate(Bt);
      }
      }

      // (ia|jb) contribution
      {
	  Ref<OrbitalSpace> cs1 = vir1_act;
	  Ref<OrbitalSpace> cs2 = vir2_act;
	  Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(cs1->rank(),cs2->rank(),-1.0);
	  std::vector<std::string> tforms_f12;
	  {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),GGspace1,cs1,GGspace2,cs2,
                                                   corrfactor(),true);
	      fill_container(tformkey_creator,tforms_f12);
	  }
	  std::vector<std::string> tforms;
	  if (!occ12_in_x12) {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),occ1_act,cs1,occ2_act,cs2,
                                                   corrfactor());
	      fill_container(tformkey_creator,tforms);
	  }
	  else
	      tforms.push_back(tforms_f12[0]);

      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,false,false>
          (V_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_eri(),
	   GGspace1, GGspace2, cs1, cs2,
	   occ1_act, occ2_act, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (X_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      if (compute_B) {
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      B_[s].scale(0.5); RefSCMatrix Bt = B_[s].t(); B_[s].accumulate(Bt);
      }
      }

      // (im|ja) contribution
      {
	  const double asymm_contr_pfac = part1_equiv_part2 ? -2.0 : -1.0;
	  Ref<OrbitalSpace> cs1 = occ1;
	  Ref<OrbitalSpace> cs2 = vir2_act;
	  Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(cs1->rank(),cs2->rank(),asymm_contr_pfac);
	  std::vector<std::string> tforms_f12;
	  {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),GGspace1,cs1,GGspace2,cs2,
                                                   corrfactor(),true);
	      fill_container(tformkey_creator,tforms_f12);
	  }
	  std::vector<std::string> tforms;
	  if (!occ12_in_x12) {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),occ1_act,cs1,occ2_act,cs2,
                                                   corrfactor());
	      fill_container(tformkey_creator,tforms);
	  }
	  else
	      tforms.push_back(tforms_f12[0]);

      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,false,false>
          (V_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_eri(),
	   GGspace1, GGspace2, cs1, cs2,
	   occ1_act, occ2_act, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (X_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      if (compute_B) {
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      B_[s].scale(0.5); RefSCMatrix Bt = B_[s].t(); B_[s].accumulate(Bt);
      }
      }

      // (im|jA) contribution
      // only needed if ansatz == 2
      if (ansatz()->projector() == R12Technology::Projector_2) {
	  const double asymm_contr_pfac = part1_equiv_part2 ? -2.0 : -1.0;
	  Ref<OrbitalSpace> cs1 = occ1;
	  Ref<OrbitalSpace> cs2 = cabs2;
	  Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(cs1->rank(),cs2->rank(),asymm_contr_pfac);
	  std::vector<std::string> tforms_f12;
	  {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),GGspace1,cs1,GGspace2,cs2,
                                                   corrfactor(),true);
	      fill_container(tformkey_creator,tforms_f12);
	  }
	  std::vector<std::string> tforms;
	  if (!occ12_in_x12) {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),occ1_act,cs1,occ2_act,cs2,
                                                   corrfactor());
	      fill_container(tformkey_creator,tforms);
	  }
	  else
	      tforms.push_back(tforms_f12[0]);

      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,false,false>
          (V_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_eri(),
	   GGspace1, GGspace2, cs1, cs2,
	   occ1_act, occ2_act, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (X_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      if (compute_B) {
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      B_[s].scale(0.5); RefSCMatrix Bt = B_[s].t(); B_[s].accumulate(Bt);
      }
      }

      if (!part1_equiv_part2) {
      // (im|ja) contribution
      {
	  const double asymm_contr_pfac = part1_equiv_part2 ? -2.0 : -1.0;
	  Ref<OrbitalSpace> cs1 = occ1;
	  Ref<OrbitalSpace> cs2 = vir2_act;
	  Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(cs1->rank(),cs2->rank(),asymm_contr_pfac);
	  std::vector<std::string> tforms_f12;
	  {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),GGspace1,cs1,GGspace2,cs2,
                                                   corrfactor(),true);
	      fill_container(tformkey_creator,tforms_f12);
	  }
	  std::vector<std::string> tforms;
	  if (!occ12_in_x12) {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),occ1_act,cs1,occ2_act,cs2,
                                                   corrfactor());
	      fill_container(tformkey_creator,tforms);
	  }
	  else
	      tforms.push_back(tforms_f12[0]);

      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,false,false>
          (V_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_eri(),
	   GGspace1, GGspace2, cs1, cs2,
	   occ1_act, occ2_act, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (X_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      if (compute_B) {
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      B_[s].scale(0.5); RefSCMatrix Bt = B_[s].t(); B_[s].accumulate(Bt);
      }
      }

      // (im|jA) contribution
      // only needed if ansatz == 2
      if (ansatz()->projector() == R12Technology::Projector_2) {
	  const double asymm_contr_pfac = part1_equiv_part2 ? -2.0 : -1.0;
	  Ref<OrbitalSpace> cs1 = occ1;
	  Ref<OrbitalSpace> cs2 = cabs2;
	  Ref<TwoParticleContraction> tpcontract = new Direct_Contraction(cs1->rank(),cs2->rank(),asymm_contr_pfac);
	  std::vector<std::string> tforms_f12;
	  {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),GGspace1,cs1,GGspace2,cs2,
                                                   corrfactor(),true);
	      fill_container(tformkey_creator,tforms_f12);
	  }
	  std::vector<std::string> tforms;
	  if (!occ12_in_x12) {
	      R12TwoBodyIntKeyCreator tformkey_creator(moints_runtime4(),occ1_act,cs1,occ2_act,cs2,
                                                   corrfactor());
	      fill_container(tformkey_creator,tforms);
	  }
	  else
	      tforms.push_back(tforms_f12[0]);

      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,false,false>
          (V_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_eri(),
	   GGspace1, GGspace2, cs1, cs2,
	   occ1_act, occ2_act, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (X_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      if (compute_B) {
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      contract_tbint_tensor<ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,ManyBodyTensors::I_to_T,true,true,false>
          (B_[s], corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
	   GGspace1, GGspace2, cs1, cs2,
	   GGspace1, GGspace2, cs1, cs2,
	   tpcontract, spincase2!=AlphaBeta, tforms_f12, tforms_f12);
      B_[s].scale(0.5); RefSCMatrix Bt = B_[s].t(); B_[s].accumulate(Bt);
      }
      }
      }

      if (!antisymmetrize && part1_equiv_part2) {
	  symmetrize<false>(V_[s],V_[s],GGspace1,occ1_act);
	  symmetrize<false>(X_[s],X_[s],GGspace1,GGspace1);
	  if (compute_B)
	      symmetrize<false>(B_[s],B_[s],GGspace1,GGspace1);
      }

      if (debug_ >= DefaultPrintThresholds::O4) {
      ExEnv::out0() << indent << __FILE__ << ": "<<__LINE__<<"\n";
	  V_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"V(diag+OBS+ABS) contribution").c_str());
	  X_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"X(diag+OBS+ABS) contribution").c_str());
	  if (compute_B)
	      B_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"B(diag+OBS+ABS) contribution").c_str());
      }

  }
}

////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
