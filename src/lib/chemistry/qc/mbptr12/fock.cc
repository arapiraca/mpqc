//
// fock.cc
//
// Copyright (C) 2004 Edward Valeev
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
// the Free Software Foundation, 67 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#include <stdexcept>
#include <util/misc/scexception.h>
#include <chemistry/qc/basis/petite.h>
#include <chemistry/qc/basis/symmint.h>
#include <chemistry/qc/basis/gaussbas.h>
#include <chemistry/qc/basis/uncontract.h>
#include <chemistry/qc/mbptr12/r12int_eval.h>
#include <chemistry/qc/mbptr12/mbptr12.h>
#include <util/misc/print.h>
#include <chemistry/qc/mbptr12/debug.h>
#include <chemistry/qc/lcao/fockbuilder.h>
#include <chemistry/qc/lcao/fockbuild_runtime.h>

using namespace std;
using namespace sc;

#define TEST_FOCKBUILD 0
#define TEST_FOCKBUILDRUNTIME 0
// set to 1 to test MVD integrals
#define TEST_DELTA_DKH 0
#define USE_PAULI 1
#define DEBUG_PRINT_ALL_F_CONTRIBUTIONS 0

RefSCMatrix
R12IntEval::fock(const Ref<OrbitalSpace>& bra_space,
                  const Ref<OrbitalSpace>& ket_space,
                  SpinCase1 spin,
                  double scale_J, double scale_K,
                  double scale_H,
                  int override_pauli)
{
  // validate the input
  if (! (scale_J == 1.0 || scale_J == 0.0 || scale_K == 1.0 || scale_K == 0.0 || scale_H == 1.0 || scale_H == 0.0) ) { // currently FockBuild can only handle unit contributions
    throw ProgrammingError("R12IntEval::fock() -- non-unit J and K coefficients are not currently supported",__FILE__,__LINE__);
  }

  if (bra_space->rank() == 0 || ket_space->rank() == 0)
    return 0;

  const Ref<GaussianBasisSet> bs1 = bra_space->basis();
  const Ref<GaussianBasisSet> bs2 = ket_space->basis();
  const bool bs1_eq_bs2 = bs1->equiv(bs2);
  int nshell1 = bs1->nshell();
  int nshell2 = bs2->nshell();

  RefSCMatrix vec1t = bra_space->coefs().t();
  RefSCMatrix vec2 = ket_space->coefs();

  RefSCMatrix F = vec1t->kit()->matrix(vec1t.rowdim(),vec2.coldim());  F.assign(0.0);

  if (scale_H != 0.0) {
    RefSCDimension aodim1 = vec1t.coldim();
    RefSCDimension aodim2 = vec2.rowdim();
    Ref<SCMatrixKit> sokit = bs1->so_matrixkit();

    Ref<RefWavefunction> reference = r12world()->refwfn();
    // Form the DK correction in the current basis using the momentum
    // basis of the reference wavefunction.  The momentum basis in the
    // reference should be a superset of hcore_basis
    Ref<GaussianBasisSet> p_basis = reference->momentum_basis();
    Ref<GaussianBasisSet> hcore_basis = (bs1_eq_bs2) ? bs1 : bs1 + bs2;

    const int dk = reference->dk();
    if (dk > 0) {
      // momentum basis in DKH calculations must span both bra and ket basis sets.
      // easiest to achieve if both are included in p_basis
      p_basis = p_basis + hcore_basis;
    }

    // include only the Pauli-Hamiltonian in the R12 treatment of the Fock operator
    bool use_pauli =
        (override_pauli == -1) ? (r12world()->r12tech()->H0_dk_approx()
            == R12Technology::H0_dk_approx_pauli_true) : override_pauli;
    if (dk == 0) {
      use_pauli = false;
    }

    RefSymmSCMatrix hsymm;
    if (use_pauli) {
      hsymm = pauli(hcore_basis);
    } else {
      // include the full DKH-Hamiltionian, or the NR-Hamiltonian, depending on the reference
      hsymm = reference->core_hamiltonian_for_basis(hcore_basis, p_basis);
    }

    // convert hsymm to the AO basis
    Ref<Integral> localints = r12world()->integral()->clone();
    localints->set_basis(hcore_basis, hcore_basis);
    Ref<PetiteList> hcore_pl = localints->petite_list();
    RefSymmSCMatrix hsymm_ao = hcore_pl->to_AO_basis(hsymm);
    hsymm = 0;

    RefSCMatrix h(aodim1, aodim2, sokit);
    if (bs1_eq_bs2) {
      h.assign(0.0);
      h.accumulate(hsymm_ao);
    } else {
      RefSCMatrix hrect_ao(hsymm_ao.dim(), hsymm_ao.dim(), sokit);
      hrect_ao.assign(0.0);
      hrect_ao.accumulate(hsymm_ao);

      // extract the bs1 by bs2 block:
      //   loop over all bs1 fblocks
      //     loop over all bs2 fblocks
      //       copy block to h
      //     end loop
      //   end loop
      const int nbf = hcore_basis->nbasis();

      GaussianBasisSetMap bs1_to_hbs(bs1, hcore_basis);
      GaussianBasisSetMap bs2_to_hbs(bs2, hcore_basis);
      const int nfblock1 = bs1_to_hbs.nfblock();
      const int nfblock2 = bs2_to_hbs.nfblock();
      for (int rb = 0; rb < nfblock1; ++rb) {
        const int rf_start1 = bs1_to_hbs.fblock_to_function(rb);
        const int rf_end1 = rf_start1 + bs1_to_hbs.fblock_size(rb) - 1;

        const int rf_start12 = bs1_to_hbs.map_function(rf_start1);
        const int rf_end12 = bs1_to_hbs.map_function(rf_end1);

        for (int cb = 0; cb < nfblock2; ++cb) {
          const int cf_start2 = bs2_to_hbs.fblock_to_function(cb);
          const int cf_end2 = cf_start2 + bs2_to_hbs.fblock_size(cb) - 1;

          const int cf_start12 =
              bs2_to_hbs.map_function(cf_start2);
          const int cf_end12 = bs2_to_hbs.map_function(cf_end2);

          // assign row-/col-subblock to h
          h.assign_subblock(hrect_ao, rf_start1, rf_end1, cf_start2, cf_end2,
                            rf_start12, cf_start12);
        }
      }

    }

    // finally, transform
    F.accumulate(vec1t * h * vec2);
    F.scale(scale_H);
//    F.print("debug Fock: Hcore");
//    if (debug_ >= DefaultPrintThresholds::allN2
//        || DEBUG_PRINT_ALL_F_CONTRIBUTIONS)
//      F.print("Core Hamiltonian contribution");
    // and clean up a bit
    h = 0;
  } // if scale_H != 0.0

  const bool spin_unrestricted = spin_polarized();

  if (!USE_FOCKBUILD) {
  //
  // add coulomb and exchange parts
  //
  const double J_prefactor = (spin_unrestricted ? 1.0 : 2.0);
  for(int s=0; s<nspincases1(); s++) {
    const SpinCase1 sc = static_cast<SpinCase1>(s);
    if (occ(sc)->rank() == 0)
      continue;
    if (scale_J != 0.0) {
      RefSCMatrix J = coulomb_(sc,bra_space,ket_space);
      J.scale(J_prefactor*scale_J);
//      if (debug_ >= DefaultPrintThresholds::allN2 || DEBUG_PRINT_ALL_F_CONTRIBUTIONS)
//        J.print("Coulomb contribution");
//      J.print("debug Fock no DF: Coulomb contribution");
      F.accumulate(J); J = 0;
    }
  }
  if (scale_K != 0.0) {
    RefSCMatrix K = exchange_(spin,bra_space,ket_space);
    K.scale(-1.0*scale_K);
//    if (debug_ >= DefaultPrintThresholds::allN2 || DEBUG_PRINT_ALL_F_CONTRIBUTIONS)
//      K.print("Exchange contribution");
//    K.print("debug Fock no DF: Exchange contribution");
    F.accumulate(K); K = 0;
  }

  } else { // USE_NEWFOCKBUILD
      Ref<FockBuildRuntime> fb_rtime = fockbuild_runtime();
      if (scale_H != 0.0) {
        const std::string hkey = ParsedOneBodyIntKey::key(bra_space->id(),ket_space->id(),std::string("H"));
        RefSCMatrix H = fb_rtime->get(hkey);
        //H.print("debug FB Fock: Core Hamiltonian contribution");
        F.assign(H*scale_H);
      }
      if (scale_J != 0.0) {
        const std::string jkey = ParsedOneBodyIntKey::key(bra_space->id(),ket_space->id(),std::string("J"));
        RefSCMatrix J = fb_rtime->get(jkey);
        //J.print("debug FB Fock: Coulomb contribution");
        F.accumulate(J*scale_J);
      }
      if (scale_K != 0.0) {
        const SpinCase1 realspin = r12world()->refwfn()->spin_polarized() ? spin : AnySpinCase1;
        const std::string kkey = ParsedOneBodyIntKey::key(bra_space->id(),ket_space->id(),std::string("K"),realspin);
        RefSCMatrix K = fb_rtime->get(kkey);
        //K.print("debug FB Fock: Exchange contribution");
        F.accumulate( (-scale_K) * K);
      }
  }

  if (debug_ >= DefaultPrintThresholds::allN2 || DEBUG_PRINT_ALL_F_CONTRIBUTIONS) {
    F.print("Fock matrix");
  }

  return F;
}


#if 0
RefSCMatrix
R12IntEval::Delta_DKH_(const Ref<OrbitalSpace>& bra_space,
                          const Ref<OrbitalSpace>& ket_space,
                          SpinCase1 spin)
{

  ExEnv::out0() << indent<< "Entering Delta_DKH" <<incindent << endl;
  const Ref<GaussianBasisSet> bs1 = bra_space->basis();
  const Ref<GaussianBasisSet> bs2 = ket_space->basis();
  const bool bs1_eq_bs2 = (bs1 == bs2);
  int nshell1 = bs1->nshell();
  int nshell2 = bs2->nshell();

  RefSCMatrix vec1t = bra_space->coefs().t();
  RefSCMatrix vec2 = ket_space->coefs();

  RefSCDimension aodim1 = vec1t.coldim();
  RefSCDimension aodim2 = vec2.rowdim();
  Ref<SCMatrixKit> sokit = bs1->so_matrixkit();

  // use R12IntEval::dk() here because deltaDKH is only needed if the MV term is treated analytically
  const int dk = this->dk();
#if !TEST_DELTA_DKH
  if (dk != 2) {
        throw ProgrammingError( "Delta_DKH can only be used with dk = 2",
 	  __FILE__, __LINE__, class_desc());
  }
#endif

  Ref<SCF> refwfn = r12world()->refwfn()->refwfn();

  Ref<GaussianBasisSet> hcore_basis = (bs1_eq_bs2) ? bs1 : bs1 + bs2;

  // Form the DK correction in the current basis using the momentum
  // basis of the reference wavefunction.  The momentum basis in the
  // reference should be a superset of hcore_basis

  // be precise, it's a critical step
  //  Ref<GaussianBasisSet> uncontract_p = new UncontractedBasisSet(refwfn->momentum_basis());
  Ref<GaussianBasisSet> uncontract_p = refwfn->momentum_basis(); // for consistency
  Ref<GaussianBasisSet> p_basis = uncontract_p+hcore_basis;

  // compute the relativistic core Hamiltonian
  RefSymmSCMatrix hsymm = refwfn->core_hamiltonian_for_basis(hcore_basis,p_basis);

  // the whole Darwin/mass_velocity block can be replaced by a call of pauli
  // I'll leave it for now for testing purposes.

#if USE_PAULI
  RefSymmSCMatrix paulicontrib = pauli(hcore_basis,p_basis);
  paulicontrib.scale(-1.0);
  hsymm.accumulate(paulicontrib);
  paulicontrib=0;

#else
  // compute Darwin term in coordinate space
  RefSymmSCMatrix Darwin(hsymm.dim(), sokit);
  Darwin.assign(0.0);
  {
    const double c = 137.0359895; // speed of light in a vacuum in a.u.
    const double darwin_prefac = M_PI/(c*c*2.0);

    mp2wfn->integral()->set_basis(hcore_basis);
    GaussianBasisSet::ValueData* vdata1 = new GaussianBasisSet::ValueData(hcore_basis,mp2wfn->integral());
    const int nbasis1 = hcore_basis->nbasis();
    double* values1=new double[nbasis1];
    const int natom=mp2wfn->molecule()->natom();

    for (int iatom=0; iatom<natom; iatom++) {

      SCVector3 R_iatom=mp2wfn->molecule()->r(iatom);
      // this puts values of basis functions evaluated at R_iatom into values1
      hcore_basis->values(R_iatom, vdata1, values1);
      const double prefac=darwin_prefac*mp2wfn->molecule()->charge(iatom);

      for (int ibasis=0; ibasis<hcore_basis->nbasis(); ibasis++) {
        for (int jbasis=0; jbasis<=ibasis; jbasis++) {

          const double d=values1[ibasis]*values1[jbasis]*prefac;
          Darwin.accumulate_element(ibasis, jbasis, d);
        }
      }
    }
    delete[] values1;
    delete vdata1;
  }

  // calculate delta_DKH = H_DKH - H_tvmvd to get the difference Hamiltonian
  // H_tvmvd = T+V+mass-velocity ...
#if TEST_DELTA_DKH
  RefSymmSCMatrix TVmv = hcore_plus_massvelocity_(hcore_basis,r12world()->basis_ri());
#else

#if EVALUATE_MV_VIA_RI
  RefSymmSCMatrix TVmv = hcore_plus_massvelocity_(hcore_basis,p_basis,true,false,false);

#else
  RefSymmSCMatrix TVmv = hcore_plus_massvelocity_(hcore_basis,p_basis);
#endif

#endif

#if !EVALUATE_MV_VIA_RI
  //         + Darwin
  TVmv.accumulate(Darwin);
#if TEST_DELTA_DKH
  {
    Ref<OrbitalSpace> occ1 = occ(Alpha);
    if (occ1->basis() == hcore_basis) {
      // to the 1-e Darwin correction to the energy at the HF level, contract with the HF density
      RefSCMatrix occ1_coefs = occ1->coefs();
      RefSCMatrix Darwin_mo = occ1_coefs.t() * Darwin * occ1_coefs;
      ExEnv::out0() << indent << "R12IntEval::hcore_plus_massvelocity_() -- 1-e Darwin term = " << 2.0 * Darwin_mo.trace() << endl;
      RefSCMatrix TVmvd_mo = occ1_coefs.t() * TVmv * occ1_coefs;
      ExEnv::out0() << indent << "R12IntEval::hcore_plus_massvelocity_() -- core + mass-velocity + 1-e Darwin term = " << 2.0 * TVmvd_mo.trace() << endl;
    }
  }
#endif

  //         + Darwin
  TVmv.accumulate(Darwin);
  Darwin=0;

  TVmv.scale(-1.0);
  hsymm.accumulate(TVmv);
#else
  hsymm.assign(TVmv);
#endif
#endif  // USE_PAULI

  // from now on proceed as in fock

  // convert hsymm to the AO basis
  Ref<Integral> localints = r12world()->integral()->clone();
  localints->set_basis(hcore_basis,hcore_basis);
  Ref<PetiteList> hcore_pl = localints->petite_list();
  RefSymmSCMatrix hsymm_ao = hcore_pl->to_AO_basis(hsymm);
  hsymm = 0;

  RefSCMatrix h(aodim1, aodim2, sokit);

  if (bs1_eq_bs2) {
    h.assign(0.0);
    h.accumulate(hsymm_ao);
  } else {
    RefSCMatrix hrect_ao(hsymm_ao.dim(), hsymm_ao.dim(), sokit);
    hrect_ao.assign(0.0);
    hrect_ao.accumulate(hsymm_ao);

    // extract the bs1 by bs2 block:
    //   loop over all bs1 fblocks
    //     loop over all bs2 fblocks
    //       copy block to h
    //     end loop
    //   end loop
    const int nbf = hcore_basis->nbasis();
    const int nfblock = bs1_plus_bs2->nfblock();
    for (int rb=0; rb<nfblock; ++rb) {
      const int rf_start12 = bs1_plus_bs2->fblock_to_function(rb);
      if (bs1_plus_bs2->function_to_basis(rf_start12) != 1)
        continue;
      const int rf_end12 = rf_start12 + bs1_plus_bs2->fblock_size(rb) - 1;

      const int rf_start1 = bs1_plus_bs2->function12_to_function(rf_start12);
      const int rf_end1 = bs1_plus_bs2->function12_to_function(rf_end12);

      for (int cb=0; cb<nfblock; ++cb) {
        const int cf_start12 = bs1_plus_bs2->fblock_to_function(cb);
        if (bs1_plus_bs2->function_to_basis(cf_start12) != 2)
          continue;
        const int cf_end12 = cf_start12 + bs1_plus_bs2->fblock_size(cb) - 1;

        const int cf_start2 = bs1_plus_bs2->function12_to_function(cf_start12);
        const int cf_end2 = bs1_plus_bs2->function12_to_function(cf_end12);

        // assign row-/col-subblock to h
        h.assign_subblock(hrect_ao, rf_start1, rf_end1, cf_start2, cf_end2,
                          rf_start12, cf_start12);
      }
    }

  }


  // finally, transform
  RefSCMatrix F = vec1t * h * vec2;
  if (debug_ >= DefaultPrintThresholds::allN2)
    F.print("Core Hamiltonian contribution");
  // and clean up a bit
  h = 0;

  const bool spin_unrestricted = spin_polarized();

  if (debug_ >= DefaultPrintThresholds::allN2) {
    F.print("Fock matrix");
  }
  ExEnv::out0() << decindent << indent << "Leaving Delta_DKH" << endl;
  return F;
}


RefSymmSCMatrix
R12IntEval::hcore_plus_massvelocity_(const Ref<GaussianBasisSet> &bas, const Ref<GaussianBasisSet> &p_bas,
                                     bool include_T, bool include_V, bool include_MV)
{
#define DK_DEBUG 0

  ExEnv::out0() << indent << "Entering hcore_plus_massvelocity_"<< incindent << endl;
  ExEnv::out0() << indent << "include_T = " << (include_T ? "true" : "false") << endl;
  ExEnv::out0() << indent << "include_V = " << (include_V ? "true" : "false") << endl;
  ExEnv::out0() << indent << "include_MV = " << (include_MV ? "true" : "false") << endl;

  // The one electron integrals will be computed in the momentum basis.
  // Use mbptr12's integrals.
  mp2wfn->integral()->set_basis(p_bas);

  Ref<PetiteList> p_pl = mp2wfn->integral()->petite_list();

  RefSCDimension p_so_dim = p_pl->SO_basisdim();
  RefSCDimension p_ao_dim = p_pl->AO_basisdim();
  Ref<SCMatrixKit> p_kit = p_bas->matrixkit();
  Ref<SCMatrixKit> p_so_kit = p_bas->so_matrixkit();

  // Compute the overlap in the momentum basis.
  RefSymmSCMatrix S_skel(p_ao_dim, p_kit);
  S_skel.assign(0.0);
  Ref<SCElementOp> hc =
    new OneBodyIntOp(new SymmOneBodyIntIter(mp2wfn->integral()->overlap(), p_pl));
  S_skel.element_op(hc);
  hc=0;
  RefSymmSCMatrix S(p_so_dim, p_so_kit);
  p_pl->symmetrize(S_skel,S);

  ExEnv::out0() << indent
                << "The momentum basis is:"
                << std::endl;
  ExEnv::out0() << incindent;
  p_bas->print_brief(ExEnv::out0());
  ExEnv::out0() << decindent;

  ExEnv::out0() << indent
                << "Orthogonalizing the momentum basis"
                << std::endl;
  Ref<OverlapOrthog> p_orthog
    = new OverlapOrthog(mp2wfn->orthog_method(), S, p_so_kit,
                        mp2wfn->lindep_tol(), debug_);

  RefSCDimension p_oso_dim = p_orthog->orthog_dim();

  // form skeleton Hcore in the momentum basis
  RefSymmSCMatrix T_skel(p_ao_dim, p_kit);
  T_skel.assign(0.0);

  hc =
    new OneBodyIntOp(new SymmOneBodyIntIter(mp2wfn->integral()->kinetic(), p_pl));
  T_skel.element_op(hc);
  hc=0;

  // finish constructing the kinetic energy integrals,
  // for which the skeleton is in hao
  RefSymmSCMatrix T(p_so_dim, p_so_kit);
  p_pl->symmetrize(T_skel,T);
  T_skel = 0;

  // Transform T into an orthogonal basis
  RefSymmSCMatrix T_oso(p_oso_dim, p_so_kit);
  T_oso.assign(0.0);
  T_oso.accumulate_transform(p_orthog->basis_to_orthog_basis(),T);

  // diagonalize the T integrals to get a momentum basis
  RefDiagSCMatrix Tval(p_oso_dim, p_so_kit);
  RefSCMatrix Tvec(p_oso_dim, p_oso_dim, p_so_kit);
  // Tvec * Tval * Tvec.t() = T_oso
  T_oso.diagonalize(Tval,Tvec);

  T_oso = 0;

#if DK_DEBUG
  T.print("T");
  Tval.print("Tval");
#endif

  // Compute the kinetic energy and the mass-velocity term
  RefSymmSCMatrix kinetic_energy(p_oso_dim, p_so_kit);
  RefSymmSCMatrix mass_velocity(p_oso_dim, p_so_kit);
  kinetic_energy.assign(0.0);
  mass_velocity.assign(0.0);
  const double c = 137.0359895; // speed of light in a vacuum in a.u.
  //  const double c = 137.035999679;  // speed of light according to CODATA (2006)
  //  const double c = 137.03599911;  // speed of light according to CRC
  int noso = p_oso_dim.n();
  for (int i=0; i<noso; i++) {
    double T_val = Tval(i);
    // momentum basis sets with near linear dependencies may
    // have T_val approximately equal to zero.  These can be
    // negative, which will cause a SIGFPE in sqrt.
    if (T_val < DBL_EPSILON) T_val = 0.0;
    const double p = sqrt(2.0*T_val);
    const double p2 = p*p;
    kinetic_energy(i,i) = 0.5*p2;
    mass_velocity(i,i)  = p2*p2 * (-1.0)/(8.0*c*c);
  }

  // Construct the transform from the coordinate to the momentum
  // representation in the momentum basis
  RefSCMatrix so_to_p
    = Tvec.t() * p_orthog->basis_to_orthog_basis();

#if DK_DEBUG
  so_to_p.print("so_to_p");
#endif

  // compute the V integrals
  Ref<OneBodyInt> V_obi = mp2wfn->integral()->nuclear();
  V_obi->reinitialize();
  hc = new OneBodyIntOp(new SymmOneBodyIntIter(V_obi, p_pl));
  RefSymmSCMatrix V_skel(p_ao_dim, p_kit);
  V_skel.assign(0.0);
  V_skel.element_op(hc);
  V_obi = 0;
  hc = 0;
  RefSymmSCMatrix V(p_so_dim, p_so_kit);
  p_pl->symmetrize(V_skel,V);
  V_skel = 0;

#if DK_DEBUG
  V.print("V");
#endif

  // transform V to the momentum basis
  RefSymmSCMatrix V_pbas(p_oso_dim, p_so_kit);
  V_pbas.assign(0.0);
  V_pbas.accumulate_transform(so_to_p, V);

  // calculate tvp
  RefSymmSCMatrix tvp(p_oso_dim, p_so_kit);
  tvp.assign(0.0);
  if (include_T)
    tvp.accumulate(kinetic_energy);
  if (include_MV)
    tvp.accumulate(mass_velocity);
  if (include_V)
    tvp.accumulate(V_pbas);

  V_pbas = 0;
  kinetic_energy=0;
  mass_velocity=0;

  // form the momentum basis hamiltonian
  RefSymmSCMatrix h_pbas(p_oso_dim, p_so_kit);
  h_pbas.assign(tvp);

#if DK_DEBUG
  h_pbas.print("h_pbas");
#endif

  // Construct the transform from the momentum representation to the
  // coordinate representation in the momentum basis
  RefSCMatrix p_to_so
    = p_orthog->basis_to_orthog_basis_inverse() * Tvec;

  // Construct the transform from the momentum basis to the
  // coordinate basis.
  mp2wfn->integral()->set_basis(bas,p_bas);
  Ref<PetiteList> pl = mp2wfn->integral()->petite_list();
  RefSCMatrix S_ao_p(pl->AO_basisdim(), p_ao_dim, p_kit);
  S_ao_p.assign(0.0);
  hc = new OneBodyIntOp(mp2wfn->integral()->overlap());
  S_ao_p.element_op(hc);
  hc=0;
  // convert s_ao_p into the so ao and so p basis
  RefSCMatrix blocked_S_ao_p(pl->AO_basisdim(), p_pl->AO_basisdim(), p_so_kit);
  blocked_S_ao_p->convert(S_ao_p);
  RefSCMatrix S_ao_p_so_l = pl->sotoao() * blocked_S_ao_p;
  RefSCMatrix S_ao_p_so = S_ao_p_so_l * p_pl->aotoso();
  S_ao_p_so_l = 0;

  // transform h_pbas back to the so basis
  RefSymmSCMatrix h_dk_so(pl->SO_basisdim(),bas->so_matrixkit());
  h_dk_so.assign(0.0);
  h_dk_so.accumulate_transform(S_ao_p_so
                               *p_orthog->overlap_inverse()
                               *p_to_so, h_pbas);

  mp2wfn->integral()->set_basis(mp2wfn->basis());
  Ref<PetiteList> bas_pl = mp2wfn->integral()->petite_list();

#if 0
  // Check to see if the momentum basis spans the coordinate basis.  The
  // following approach seems reasonable, but a more careful mathematical
  // analysis would be desirable.
  double S_ao_projected_trace
    = (S_ao_p_so * p_orthog->overlap_inverse() * S_ao_p_so.t()).trace()
    / pl->SO_basisdim()->n();
  double S_ao_trace = mp2wfn->overlap().trace() / bas_pl->SO_basisdim()->n();
  ExEnv::out0() << indent
                << "Tr(orbital basis overlap)/N = "
                << S_ao_trace
                << std::endl;
  ExEnv::out0() << indent
                << "Tr(orbital basis overlap projected into momentum basis)/N = "
                << S_ao_projected_trace
                << std::endl;
  if (fabs(S_ao_projected_trace-S_ao_trace)>mp2wfn->lindep_tol()) {
    ExEnv::out0() << indent
                  << "WARNING: the momentum basis does not span the orbital basis"
                  << std::endl;
  }
#endif

#if DK_DEBUG
  S_ao_p_so.print("S_ao_p_so");
  p_to_so.print("p_to_so");
  //(p_to_so*so_to_p).print("p_to_so*so_to_p");
  (S_ao_p_so*S.gi()*p_to_so).print("S_ao_p_so*S.gi()*p_to_so");
  T.print("T");
  (T+V).print("T+V");
  h_dk_so.print("h_dk_so");
#endif

  ExEnv::out0() << decindent << indent << "Leaving hcore_plus_massvelocity_" << endl;
  return h_dk_so;
}
#endif

RefSymmSCMatrix
R12IntEval::pauli(
  const Ref<GaussianBasisSet> &basis,
  const Ref<GaussianBasisSet> &pbas,
  const bool momentum)
{
  RefSymmSCMatrix hcore;

  if (momentum) {
    hcore = pauli_momentumspace(basis, pbas);
  }
  else {
    hcore = pauli_realspace(basis);
  }

  return hcore;
}


RefSymmSCMatrix
R12IntEval::pauli_realspace(const Ref<GaussianBasisSet> &bas)
{
  const double c = 137.0359895; // speed of light in a vacuum in a.u.

  Ref<Integral> localints = r12world()->integral()->clone();
  localints->set_basis(bas);

  Ref<PetiteList> pl = localints->petite_list();

  // form skeleton Hcore in AO basis
  RefSymmSCMatrix hao(bas->basisdim(), bas->matrixkit());
  hao.assign(0.0);

  // kinetic energy
  Ref<SCElementOp> hc =
    new OneBodyIntOp(new SymmOneBodyIntIter(localints->kinetic(), pl));
  hao.element_op(hc);
  hc=0;

  // molecular potential
  Ref<OneBodyInt> nuc = localints->nuclear();
  nuc->reinitialize();
  hc = new OneBodyIntOp(new SymmOneBodyIntIter(nuc, pl));
  hao.element_op(hc);
  hc=0;

  // mass-velocity
  Ref<OneBodyInt> mv = localints->p4();
  mv->reinitialize();
  hc = new OneBodyIntOp(new SymmOneBodyIntIter(mv, pl));
  RefSymmSCMatrix mv_ao(bas->basisdim(), bas->matrixkit());
  mv_ao.assign(0.0);
  mv_ao.element_op(hc);
  mv_ao.scale((-1.0)/(8.0*c*c));
  hao.accumulate(mv_ao);
  mv_ao=0;
  hc=0;

  // Darwin term
  RefSymmSCMatrix Darwin(bas->basisdim(), bas->matrixkit());
  Darwin.assign(0.0);
  {
    const double darwin_prefac = M_PI/(c*c*2.0);
    Wavefunction *wfn = r12world()->wfn();

    GaussianBasisSet::ValueData* vdata1 = new GaussianBasisSet::ValueData(bas,localints);
    const int nbasis1 = bas->nbasis();
    double* values1=new double[nbasis1];
    const int natom=wfn->molecule()->natom();

    for (int iatom=0; iatom<natom; iatom++) {

      SCVector3 R_iatom=wfn->molecule()->r(iatom);
      // this puts values of basis functions evaluated at R_iatom into values1
      bas->values(R_iatom, vdata1, values1);
      const double prefac=darwin_prefac*wfn->molecule()->charge(iatom);

      for (int ibasis=0; ibasis<bas->nbasis(); ibasis++) {
        for (int jbasis=0; jbasis<=ibasis; jbasis++) {

          const double d=values1[ibasis]*values1[jbasis]*prefac;
          Darwin.accumulate_element(ibasis, jbasis, d);
        }
      }
    }
    delete[] values1;
    delete vdata1;
  }
  hao.accumulate(Darwin);


  // now symmetrize Hso
  RefSymmSCMatrix h(pl->SO_basisdim(), bas->so_matrixkit());
  pl->symmetrize(hao,h);

//  h.print("pauli_realspace");

  return h;
}


RefSymmSCMatrix
R12IntEval::pauli_momentumspace(const Ref<GaussianBasisSet> &bas, const Ref<GaussianBasisSet> &p_bas)
{
  Wavefunction *wfn = r12world()->wfn();
#define PAULI_DEBUG 0

  ExEnv::out0() << indent<< "Using the Pauli Hamiltonian for the R12 treatment " << endl;

  // The one electron integrals will be computed in the momentum basis.
  // Use mbptr12's integrals.
  wfn->integral()->set_basis(p_bas);

  Ref<PetiteList> p_pl = wfn->integral()->petite_list();

  RefSCDimension p_so_dim = p_pl->SO_basisdim();
  RefSCDimension p_ao_dim = p_pl->AO_basisdim();
  Ref<SCMatrixKit> p_kit = p_bas->matrixkit();
  Ref<SCMatrixKit> p_so_kit = p_bas->so_matrixkit();

  // Compute the overlap in the momentum basis.
  RefSymmSCMatrix S_skel(p_ao_dim, p_kit);
  S_skel.assign(0.0);
  Ref<SCElementOp> hc =
    new OneBodyIntOp(new SymmOneBodyIntIter(wfn->integral()->overlap(), p_pl));
  S_skel.element_op(hc);
  hc=0;
  RefSymmSCMatrix S(p_so_dim, p_so_kit);
  p_pl->symmetrize(S_skel,S);

  ExEnv::out0() << indent
                << "The momentum basis is:"
                << std::endl;
  ExEnv::out0() << incindent;
  p_bas->print_brief(ExEnv::out0());
  ExEnv::out0() << decindent;

  ExEnv::out0() << indent
                << "Orthogonalizing the momentum basis"
                << std::endl;
  Ref<OverlapOrthog> p_orthog
    = new OverlapOrthog(wfn->orthog_method(), S, p_so_kit,
                        wfn->lindep_tol(), debug_);

  RefSCDimension p_oso_dim = p_orthog->orthog_dim();

  // form skeleton Hcore in the momentum basis
  RefSymmSCMatrix T_skel(p_ao_dim, p_kit);
  T_skel.assign(0.0);

  hc =
    new OneBodyIntOp(new SymmOneBodyIntIter(wfn->integral()->kinetic(), p_pl));
  T_skel.element_op(hc);
  hc=0;

  // finish constructing the kinetic energy integrals,
  // for which the skeleton is in hao
  RefSymmSCMatrix T(p_so_dim, p_so_kit);
  p_pl->symmetrize(T_skel,T);
  T_skel = 0;

  // Transform T into an orthogonal basis
  RefSymmSCMatrix T_oso(p_oso_dim, p_so_kit);
  T_oso.assign(0.0);
  T_oso.accumulate_transform(p_orthog->basis_to_orthog_basis(),T);

  // diagonalize the T integrals to get a momentum basis
  RefDiagSCMatrix Tval(p_oso_dim, p_so_kit);
  RefSCMatrix Tvec(p_oso_dim, p_oso_dim, p_so_kit);
  // Tvec * Tval * Tvec.t() = T_oso
  T_oso.diagonalize(Tval,Tvec);

  T_oso = 0;

#if PAULI_DEBUG
  T.print("T");
  Tval.print("Tval");
#endif

  // Compute the kinetic energy and the mass-velocity term
  RefSymmSCMatrix kinetic_energy(p_oso_dim, p_so_kit);
  RefSymmSCMatrix mass_velocity(p_oso_dim, p_so_kit);
  kinetic_energy.assign(0.0);
  mass_velocity.assign(0.0);
  const double c = 137.0359895; // speed of light in a vacuum in a.u.
  //  const double c = 137.035999679;  // speed of light according to CODATA (2006)
  //  const double c = 137.03599911;  // speed of light according to CRC
  int noso = p_oso_dim.n();
  for (int i=0; i<noso; i++) {
    double T_val = Tval(i);
    // momentum basis sets with near linear dependencies may
    // have T_val approximately equal to zero.  These can be
    // negative, which will cause a SIGFPE in sqrt.
    if (T_val < DBL_EPSILON) T_val = 0.0;
    const double p = sqrt(2.0*T_val);
    const double p2 = p*p;
    kinetic_energy(i,i) = 0.5*p2;
    mass_velocity(i,i)  = p2*p2 * (-1.0)/(8.0*c*c);
  }

  // Construct the transform from the coordinate to the momentum
  // representation in the momentum basis
  RefSCMatrix so_to_p
    = Tvec.t() * p_orthog->basis_to_orthog_basis();

#if PAULI_DEBUG
  so_to_p.print("so_to_p");
#endif

  // compute the V integrals
  Ref<OneBodyInt> V_obi = wfn->integral()->nuclear();
  V_obi->reinitialize();
  hc = new OneBodyIntOp(new SymmOneBodyIntIter(V_obi, p_pl));
  RefSymmSCMatrix V_skel(p_ao_dim, p_kit);
  V_skel.assign(0.0);
  V_skel.element_op(hc);
  V_obi = 0;
  hc = 0;
  RefSymmSCMatrix V(p_so_dim, p_so_kit);
  p_pl->symmetrize(V_skel,V);
  V_skel = 0;

#if PAULI_DEBUG
  V.print("V");
#endif

  // transform V to the momentum basis
  RefSymmSCMatrix V_pbas(p_oso_dim, p_so_kit);
  V_pbas.assign(0.0);
  V_pbas.accumulate_transform(so_to_p, V);

  // calculate tvp
  RefSymmSCMatrix tvp(p_oso_dim, p_so_kit);
  tvp.assign(0.0);
  tvp.accumulate(kinetic_energy);
  tvp.accumulate(mass_velocity);
  tvp.accumulate(V_pbas);

  V_pbas = 0;
  kinetic_energy=0;
  mass_velocity=0;

  // form the momentum basis hamiltonian
  RefSymmSCMatrix h_pbas(p_oso_dim, p_so_kit);
  h_pbas.assign(tvp);

#if PAULI_DEBUG
  h_pbas.print("h_pbas");
#endif

  // Construct the transform from the momentum representation to the
  // coordinate representation in the momentum basis
  RefSCMatrix p_to_so
    = p_orthog->basis_to_orthog_basis_inverse() * Tvec;

  // Construct the transform from the momentum basis to the
  // coordinate basis.
  wfn->integral()->set_basis(bas,p_bas);
  Ref<PetiteList> pl = wfn->integral()->petite_list();
  RefSCMatrix S_ao_p(pl->AO_basisdim(), p_ao_dim, p_kit);
  S_ao_p.assign(0.0);
  hc = new OneBodyIntOp(wfn->integral()->overlap());
  S_ao_p.element_op(hc);
  hc=0;
  // convert s_ao_p into the so ao and so p basis
  RefSCMatrix blocked_S_ao_p(pl->AO_basisdim(), p_pl->AO_basisdim(), p_so_kit);
  blocked_S_ao_p->convert(S_ao_p);
  RefSCMatrix S_ao_p_so_l = pl->sotoao() * blocked_S_ao_p;
  RefSCMatrix S_ao_p_so = S_ao_p_so_l * p_pl->aotoso();
  S_ao_p_so_l = 0;

  // transform h_pbas back to the so basis
  RefSymmSCMatrix h_dk_so(pl->SO_basisdim(),bas->so_matrixkit());
  h_dk_so.assign(0.0);
  h_dk_so.accumulate_transform(S_ao_p_so
                               *p_orthog->overlap_inverse()
                               *p_to_so, h_pbas);

  wfn->integral()->set_basis(wfn->basis());
  Ref<PetiteList> bas_pl = wfn->integral()->petite_list();

#if 0
  // Check to see if the momentum basis spans the coordinate basis.  The
  // following approach seems reasonable, but a more careful mathematical
  // analysis would be desirable.
  double S_ao_projected_trace
    = (S_ao_p_so * p_orthog->overlap_inverse() * S_ao_p_so.t()).trace()
    / pl->SO_basisdim()->n();
  double S_ao_trace = wfn->overlap().trace() / bas_pl->SO_basisdim()->n();
  ExEnv::out0() << indent
                << "Tr(orbital basis overlap)/N = "
                << S_ao_trace
                << std::endl;
  ExEnv::out0() << indent
                << "Tr(orbital basis overlap projected into momentum basis)/N = "
                << S_ao_projected_trace
                << std::endl;
  if (fabs(S_ao_projected_trace-S_ao_trace)>wfn->lindep_tol()) {
    ExEnv::out0() << indent
                  << "WARNING: the momentum basis does not span the orbital basis"
                  << std::endl;
  }
#endif

#if PAULI_DEBUG
  S_ao_p_so.print("S_ao_p_so");
  p_to_so.print("p_to_so");
  //(p_to_so*so_to_p).print("p_to_so*so_to_p");
  (S_ao_p_so*S.gi()*p_to_so).print("S_ao_p_so*S.gi()*p_to_so");
  T.print("T");
  (T+V).print("T+V");
  h_dk_so.print("h_dk_so");
#endif


  // Finally construct the Darwin term
  RefSymmSCMatrix Darwin(h_dk_so.dim(), bas->so_matrixkit());
  Darwin.assign(0.0);
  {
    const double darwin_prefac = M_PI/(c*c*2.0);

    wfn->integral()->set_basis(bas);
    GaussianBasisSet::ValueData* vdata1 = new GaussianBasisSet::ValueData(bas,wfn->integral());
    const int nbasis1 = bas->nbasis();
    double* values1=new double[nbasis1];
    const int natom=wfn->molecule()->natom();

    for (int iatom=0; iatom<natom; iatom++) {

      SCVector3 R_iatom=wfn->molecule()->r(iatom);
      // this puts values of basis functions evaluated at R_iatom into values1
      bas->values(R_iatom, vdata1, values1);
      const double prefac=darwin_prefac*wfn->molecule()->charge(iatom);

      for (int ibasis=0; ibasis<bas->nbasis(); ibasis++) {
        for (int jbasis=0; jbasis<=ibasis; jbasis++) {

          const double d=values1[ibasis]*values1[jbasis]*prefac;
          Darwin.accumulate_element(ibasis, jbasis, d);
        }
      }
    }
    delete[] values1;
    delete vdata1;
  }
#if PAULI_DEBUG
  Darwin.print("Darwin term");
#endif
  h_dk_so.accumulate(Darwin);

//  h_dk_so.print("pauli_momentumspace");

  return h_dk_so;
}



///////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// c-file-style: "CLJ"
// End:
