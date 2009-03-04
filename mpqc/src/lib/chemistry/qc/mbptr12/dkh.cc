//
// dkh.cc
//
// Copyright (C) 2008 Edward Valeev
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
#include <chemistry/qc/basis/petite.h>

// tests against analytic results from Mathematica suggest that M contribution comes out too large by a factor of 2
// TODO: figure out where we made an apparent error for a factor of 2
#define P_INCLUDES_Mover2 1

using namespace std;
using namespace sc;

namespace sc {
namespace detail {

  // produces kinetic energy-transformed intspace
  Ref<OrbitalSpace> t_x_X(const Ref<OrbitalSpace>& extspace,
                          const Ref<OrbitalSpace>& intspace) {

    if (!extspace->integral()->equiv(intspace->integral()))
      throw ProgrammingError("two OrbitalSpaces use incompatible Integral factories");
    const Ref<GaussianBasisSet> bs1 = extspace->basis();
    const Ref<GaussianBasisSet> bs2 = intspace->basis();
    const bool bs1_eq_bs2 = (bs1 == bs2);
    int nshell1 = bs1->nshell();
    int nshell2 = bs2->nshell();

    RefSCMatrix vec1t = extspace->coefs().t();
    RefSCMatrix vec2 = intspace->coefs();

    Ref<Integral> localints = extspace->integral()->clone();
    localints->set_basis(bs1,bs2);
#if 1
    Ref<SCElementOp> op = new OneBodyIntOp(localints->kinetic());
#else
    Ref<SCElementOp> op = new OneBodyIntOp(localints->overlap());
#endif
    RefSCDimension aodim1 = vec1t.coldim();
    RefSCDimension aodim2 = vec2.rowdim();
    Ref<SCMatrixKit> aokit = bs1->so_matrixkit();
    RefSCMatrix t12_ao(aodim1, aodim2, aokit);
    t12_ao.assign(0.0);
    t12_ao.element_op(op);
    op = 0;

    // finally, transform
    RefSCMatrix t12 = vec1t * t12_ao * vec2;
    t12_ao = 0;

    RefSCMatrix t12_coefs = intspace->coefs() * t12.t();
    std::string id = extspace->id();  id += "_T(";  id += intspace->id();  id += ")";
    ExEnv::out0() << "id = " << id << endl;
    std::string name = "(T)-weighted space";
    Ref<OrbitalSpace> result = new OrbitalSpace(id, name, extspace, t12_coefs, bs2);

    return result;
  }

}}

void R12IntEval::compute_B_DKH_() {
  if (evaluated_)
    return;

  const bool obs_eq_vbs = r12info()->obs_eq_vbs();
  const bool obs_eq_ribs = r12info()->obs_eq_ribs();
  const unsigned int maxnabs = r12info()->maxnabs();

  // Check if the requested calculation is implemented
  if (!obs_eq_vbs && maxnabs < 1)
    throw FeatureNotImplemented("OBS!=VBS & maxnabs == 0 is not supported yet in relativistic calculations",__FILE__,__LINE__);

  Timer tim_B_DKH2("Analytic double-commutator B(DKH2) intermediate");
  ExEnv::out0() << endl << indent
      << "Entered analytic double-commutator B(DKH2) intermediate evaluator" << endl;
  ExEnv::out0() << incindent;

  const double c = 137.0359895;
  const double minus_one_over_8c2 = -1.0 / (8.0 * c * c);

  //
  // Compute kinetic energy integrals and obtain geminal-generator spaces transformed with them
  //
  Ref<OrbitalSpace> t_x_P[NSpinCases1];
  Ref<OrbitalSpace> rispace = (maxnabs < 1) ? r12info()->refinfo()->orbs() : r12info()->ribs_space();
  for(int s=0; s<NSpinCases1; ++s) {
    using namespace sc::LinearR12;
    const SpinCase1 spin = static_cast<SpinCase1>(s);
    Ref<OrbitalSpace> x = xspace(spin);

    t_x_P[s] = sc::detail::t_x_X(x, rispace);
    OrbitalSpaceRegistry::instance()->add(make_keyspace_pair(t_x_P[s]));
  }

  // Loop over every 2-e spincase
  for (int s=0; s<nspincases2(); s++) {
    using namespace sc::LinearR12;
    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    const SpinCase1 spin1 = case1(spincase2);
    const SpinCase1 spin2 = case2(spincase2);
    Ref<SingleRefInfo> refinfo = r12info()->refinfo();

    const Ref<OrbitalSpace>& xspace1 = xspace(spin1);
    const Ref<OrbitalSpace>& xspace2 = xspace(spin2);
    const bool x1_eq_x2 = (xspace1 == xspace2);

    // are particles 1 and 2 equivalent?
    const bool part1_equiv_part2 = (spincase2 != AlphaBeta) || x1_eq_x2;
    // Need to antisymmetrize 1 and 2
    const bool antisymmetrize = (spincase2 != AlphaBeta);

    RefSCMatrix B_DKH = B_[s].clone();
    B_DKH.assign(0.0);

    Ref<OrbitalSpace> t_x1 = t_x_P[spin1];
    Ref<OrbitalSpace> t_x2 = t_x_P[spin2];

    // <xy|z T> tforms
    std::vector<std::string> tforms_xyzT;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(r12info()->moints_runtime(), xspace1, xspace1, xspace2,
          t_x2, r12info()->corrfactor(), true, true);
      fill_container(tformkey_creator, tforms_xyzT);
    }
    compute_tbint_tensor<ManyBodyTensors::I_to_T, true, true>(
        B_DKH,
        corrfactor()->tbint_type_f12t1f12(),
        xspace1, xspace1,
        xspace2, t_x2,
        antisymmetrize,
        tforms_xyzT);
    if (!part1_equiv_part2) {
      // <xy|T z> tforms
      std::vector<std::string> tforms_xyTz;
      {
        R12TwoBodyIntKeyCreator tformkey_creator(r12info()->moints_runtime(), xspace1, t_x1, xspace2,
            xspace2, r12info()->corrfactor(), true, true);
        fill_container(tformkey_creator, tforms_xyTz);
      }
      compute_tbint_tensor<ManyBodyTensors::I_to_T, true, true>(
          B_DKH,
          corrfactor()->tbint_type_f12t1f12(),
          xspace1, t_x1,
          xspace2, xspace2,
          antisymmetrize,
          tforms_xyTz);
    }
    else {
      B_DKH.scale(2.0);
      if (spincase2 == AlphaBeta) {
        symmetrize<false>(B_DKH,B_DKH,xspace1,xspace1);
      }
    }

    // symmetrize bra and ket
    B_DKH.scale(0.5);
    RefSCMatrix B_DKH_t = B_DKH.t();
    B_DKH.accumulate(B_DKH_t);  B_DKH_t = 0;

    // and scale by the prefactor
    // M1 = 2 ( f12(T1+T2)f12 (T1 + T2) + (T1 + T2) f12(T1+T2)f12 ) = 4 * ( f12T1f12 (T1 + T2) + (T1 + T2) f12T1f12 )
    // what I have computed so far is 1/2 * (f12T1f12 (T1 + T2) + (T1 + T2) f12T1f12)
    // hence multiply by 8
    B_DKH.scale(8.0 * minus_one_over_8c2);

#if P_INCLUDES_Mover2
    B_DKH.scale(0.5);
#endif

    if (debug_ >= DefaultPrintThresholds::O4) {
      B_DKH.print(prepend_spincase(spincase2,"B(DKH2) contribution (M1)").c_str());
    }
    B_[s].accumulate(B_DKH);
    B_DKH.assign(0.0);

    // Transforms for this type of integrals does not yet exists
    // will not use RangeCreator here because its input is hardwired to corrfactor()->tbintdescr()
    // will create a set of descriptors so that compute_tbint_tensor can construct transforms
    // only 1 correlation factor of G12 type is accepted at the moment, hence completely manual work here
    std::vector<std::string> tforms_g12dkh;
    if (corrfactor()->nfunctions() > 1)
      throw FeatureNotImplemented("B(DKH2) evaluator can only work with one correlation factor",__FILE__,__LINE__);
    Ref<G12CorrelationFactor> g12corrfact;   g12corrfact << corrfactor();
    Ref<G12NCCorrelationFactor> g12nccorrfact; g12nccorrfact << corrfactor();
    if (g12nccorrfact.null() && g12corrfact.null())
      throw FeatureNotImplemented("B(DKH2) evaluator can only work with Gaussian (or Gaussian-expanded) correlation factors",__FILE__,__LINE__);
    Ref<TwoBodyIntDescr> descr_g12dkh;
    if (g12corrfact.nonnull())
      descr_g12dkh = new TwoBodyIntDescrG12DKH(r12info()->integral(),
                                               new IntParamsG12(g12corrfact->function(0),
                                                                g12corrfact->function(0))
      );
    else
      descr_g12dkh = new TwoBodyIntDescrG12DKH(r12info()->integral(),
                                               new IntParamsG12(g12nccorrfact->function(0),
                                                                g12nccorrfact->function(0))
      );
    const std::string descr_key = r12info()->moints_runtime()->descr_key(descr_g12dkh);
    const std::string tform_key = ParsedTwoBodyIntKey::key(xspace1->id(),xspace2->id(),
                                                           xspace1->id(),xspace2->id(),
                                                           descr_key,
                                                           std::string(MOIntsRuntime::Layout_b1b2_k1k2));
    tforms_g12dkh.push_back(tform_key);

    // M2H + M3H
    compute_tbint_tensor<ManyBodyTensors::I_to_T, true, true>(
                                                              B_DKH,
                                                              TwoBodyInt::g12p4g12_m_g12t1g12t1,
                                                              xspace1, xspace1,
                                                              xspace2, xspace2,
                                                              antisymmetrize,
                                                              tforms_g12dkh);
    B_DKH.scale(minus_one_over_8c2);
#if P_INCLUDES_Mover2
    B_DKH.scale(0.5);
#endif
    if (debug_ >= DefaultPrintThresholds::O4) {
      B_DKH.print(prepend_spincase(spincase2,"B(DKH2) contribution (M2+M3)").c_str());
    }
    B_[s].accumulate(B_DKH);
    B_DKH.assign(0.0);

  } // end of spincase2 loop

  ExEnv::out0() << decindent;
  ExEnv::out0() << indent << "Exited analytic double-commutator B(DKH2) intermediate evaluator"
      << endl;

  tim_B_DKH2.exit();
  checkpoint_();

  return;
}

void R12IntEval::contrib_to_B_DKH_a_() {
  if (evaluated_)
    return;

  const bool obs_eq_vbs = r12info()->obs_eq_vbs();
  const bool obs_eq_ribs = r12info()->obs_eq_ribs();

  // Check if the requested calculation is implemented
  if (obs_eq_ribs)
    throw FeatureNotImplemented("OBS==RIBS is not supported yet in relativistic calculations",__FILE__,__LINE__);

  Timer tim_B_DKH2("Analytic single-commutator B(DKH2) intermediate");
  ExEnv::out0() << endl << indent
      << "Entered analytic single-commutator B(DKH2) intermediate evaluator" << endl;
  ExEnv::out0() << incindent;

  const double c = 137.0359895;
  const double minus_one_over_8c2 = -1.0 / (8.0 * c * c);

  //
  // Compute kinetic energy integrals and x_t(A) spaces
  //
  Ref<OrbitalSpace> t_x_P[NSpinCases1];
  Ref<OrbitalSpace> t_p_P[NSpinCases1];
  Ref<OrbitalSpace> t_x_A[NSpinCases1];
  for(int s=0; s<NSpinCases1; ++s) {
    using namespace sc::LinearR12;
    const SpinCase1 spin = static_cast<SpinCase1>(s);
    Ref<OrbitalSpace> x = xspace(spin);
    Ref<OrbitalSpace> obs = r12info()->refinfo()->orbs(spin);
    Ref<OrbitalSpace> ribs = r12info()->ribs_space();
    Ref<OrbitalSpace> cabs = r12info()->ribs_space(spin);

    t_x_P[s] = sc::detail::t_x_X(x, ribs);
    OrbitalSpaceRegistry::instance()->add(make_keyspace_pair(t_x_P[s]));

    t_p_P[s] = sc::detail::t_x_X(obs, ribs);
    OrbitalSpaceRegistry::instance()->add(make_keyspace_pair(t_p_P[s]));

    t_x_A[s] = sc::detail::t_x_X(x, cabs);
    OrbitalSpaceRegistry::instance()->add(make_keyspace_pair(t_x_A[s]));
  }

  // Loop over every 2-e spincase
  for (int s=0; s<nspincases2(); s++) {
    using namespace sc::LinearR12;
    const SpinCase2 spincase2 = static_cast<SpinCase2>(s);
    const SpinCase1 spin1 = case1(spincase2);
    const SpinCase1 spin2 = case2(spincase2);
    Ref<SingleRefInfo> refinfo = r12info()->refinfo();

    const Ref<OrbitalSpace>& xspace1 = xspace(spin1);
    const Ref<OrbitalSpace>& xspace2 = xspace(spin2);
    const Ref<OrbitalSpace>& orbs1 = refinfo->orbs(spin1);
    const Ref<OrbitalSpace>& orbs2 = refinfo->orbs(spin2);
    const bool x1_eq_x2 = (xspace1 == xspace2);
    const Ref<OrbitalSpace>& x_tP1 = t_x_P[spin1];
    const Ref<OrbitalSpace>& x_tP2 = t_x_P[spin2];
    const Ref<OrbitalSpace>& p_tP1 = t_p_P[spin1];
    const Ref<OrbitalSpace>& p_tP2 = t_p_P[spin2];

    // are particles 1 and 2 equivalent?
    const bool part1_equiv_part2 = (spincase2 != AlphaBeta) || x1_eq_x2;
    // Need to antisymmetrize 1 and 2
    const bool antisymmetrize = (spincase2 != AlphaBeta);

    RefSCMatrix B_DKH = B_[s].clone();
    B_DKH.assign(0.0);

    Ref<TwoParticleContraction> tpcontract = new CABS_OBS_Contraction(refinfo->orbs(spin1)->rank());
    // <x y|p q> tforms
    std::vector<std::string> tforms_xy_pq;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(
        r12info()->moints_runtime(),
        xspace1,
        orbs1,
        xspace2,
        orbs2,
        r12info()->corrfactor(),true
        );
      fill_container(tformkey_creator, tforms_xy_pq);
    }
    // <x y|p_T q> tforms
    std::vector<std::string> tforms_xy_pTq;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(
        r12info()->moints_runtime(),
        xspace1,
        p_tP1,
        xspace2,
        orbs2,
        r12info()->corrfactor(),true
        );
      fill_container(tformkey_creator, tforms_xy_pTq);
    }
    // <x y|p q_T> tforms
    std::vector<std::string> tforms_xy_pqT;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(
        r12info()->moints_runtime(),
        xspace1,
        orbs1,
        xspace2,
        p_tP2,
        r12info()->corrfactor(),true
        );
      fill_container(tformkey_creator, tforms_xy_pqT);
    }
    // <x_T y|p q> tforms
    std::vector<std::string> tforms_xTy_pq;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(
        r12info()->moints_runtime(),
        x_tP1,
        orbs1,
        xspace2,
        orbs2,
        r12info()->corrfactor(),true
        );
      fill_container(tformkey_creator, tforms_xTy_pq);
    }
    // <x y_T|p q> tforms
    std::vector<std::string> tforms_xyT_pq;
    {
      R12TwoBodyIntKeyCreator tformkey_creator(
        r12info()->moints_runtime(),
        xspace1,
        orbs1,
        x_tP2,
        orbs2,
        r12info()->corrfactor(),true
        );
      fill_container(tformkey_creator, tforms_xyT_pq);
    }

    contract_tbint_tensor<ManyBodyTensors::I_to_T,
        ManyBodyTensors::I_to_T,
        ManyBodyTensors::I_to_T,
        true,true,false>
        (
        B_DKH, corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
        xspace1, xspace2,
        orbs1, orbs2,
        xspace1, xspace2,
        p_tP1, orbs2,
        tpcontract,
        spincase2!=AlphaBeta, tforms_xy_pq, tforms_xy_pTq
        );
    contract_tbint_tensor<ManyBodyTensors::I_to_T,
        ManyBodyTensors::I_to_T,
        ManyBodyTensors::I_to_T,
        true,true,false>
        (
        B_DKH, corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
        xspace1, xspace2,
        orbs1, orbs2,
        xspace1, xspace2,
        orbs1, p_tP2,
        tpcontract,
        spincase2!=AlphaBeta, tforms_xy_pq, tforms_xy_pqT
        );

        contract_tbint_tensor<ManyBodyTensors::I_to_T,
            ManyBodyTensors::I_to_T,
            ManyBodyTensors::I_to_T,
            true,true,false>
            (
            B_DKH, corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t1f12(),
            xspace1, xspace2,
            orbs1, orbs2,
            x_tP1, xspace2,
            orbs1, orbs2,
            tpcontract,
            spincase2!=AlphaBeta, tforms_xy_pq, tforms_xTy_pq
            );
        contract_tbint_tensor<ManyBodyTensors::I_to_T,
            ManyBodyTensors::I_to_T,
            ManyBodyTensors::I_to_T,
            true,true,false>
            (
            B_DKH, corrfactor()->tbint_type_f12(), corrfactor()->tbint_type_t2f12(),
            xspace1, xspace2,
            orbs1, orbs2,
            xspace1, x_tP2,
            orbs1, orbs2,
            tpcontract,
            spincase2!=AlphaBeta, tforms_xy_pq, tforms_xyT_pq
            );

            // symmetrize bra and ket
            B_DKH.scale(0.5);
            RefSCMatrix B_DKH_t = B_DKH.t();
            B_DKH.accumulate(B_DKH_t);  B_DKH_t = 0;

            // 0.5 due to double counting contributions from electrons 1 and 2
            // 4 from p^2 -> T
            B_DKH.scale(0.5 * 4.0 * minus_one_over_8c2);

            if (debug_ >= DefaultPrintThresholds::O4) {
              globally_sum_intermeds_();
              B_DKH.print(prepend_spincase(static_cast<SpinCase2>(s),"B(DKH pq) contribution").c_str());
            }

            B_[s].accumulate(B_DKH);
            B_DKH.assign(0.0);

            B_[s].print(prepend_spincase(static_cast<SpinCase2>(s),"B(diag+OBS+ABS+MV) contribution").c_str());
    }

  ExEnv::out0() << decindent;
  ExEnv::out0() << indent << "Exited analytic single-commutator B(DKH2) intermediate evaluator"
      << endl;

  tim_B_DKH2.exit();
  checkpoint_();

  return;
}

////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ-CONDENSED"
// End:
