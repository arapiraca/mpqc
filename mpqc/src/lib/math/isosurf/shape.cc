
extern "C" {
# include <math.h>
  }

#include <math/scmat/matrix.h>
#include <math/scmat/local.h>
#include "shape.h"

// given a vector X find which of the points in the vector of
// vectors, A, is closest to it and return the distance
static double
closest_distance(SCVector3& X,SCVector3*A,int n,double*grad)
{
  SCVector3 T = X-A[0];
  double min = T.dot(T);
  int imin = 0;
  for (int i=1; i<n; i++) {
      T = X-A[i];
      double tmp = T.dot(T);
      if (tmp < min) {min = tmp; imin = i;}
    }
  if (grad) {
      T = X - A[imin];
      T.normalize();
      for (int i=0; i<3; i++) grad[i] = T[i];
    }
  return sqrt(min);
}

//////////////////////////////////////////////////////////////////////
// Shape

#define CLASSNAME Shape
#define PARENTS public Volume
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
Shape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { Volume::_castdown(cd) };
  return do_castdowns(casts,cd);
}

SavableState_REF_def(Shape);
ARRAY_def(RefShape);
SET_def(RefShape);
ARRAYSET_def(RefShape);

Shape::Shape():
  Volume(new LocalSCDimension(3))
{
}

Shape::~Shape()
{
}

void
Shape::compute()
{
  RefSCVector cv = get_x();
  SCVector3 r;
  r[0] = cv.get_element(0);
  r[1] = cv.get_element(1);
  r[2] = cv.get_element(2);
  if (do_gradient()) {
      double v[3];
      set_value(distance_to_surface(r,v));
      RefSCVector vv(dimension());
      vv.assign(v);
      set_gradient(vv);
    }
  else if (do_value()) set_value(distance_to_surface(r));
  if (do_hessian()) {
      fprintf(stderr,"Shape::compute(): can't do hessian yet\n");
      abort();
    }
}

int
Shape::is_outside(const SCVector3&r) const
{
  if (distance_to_surface(r)>0.0) return 1;
  return 0;
}

// Shape overrides volume's interpolate so it always gets points on
// the outside of the shape are always returned.

// interpolate using the bisection algorithm
RefSCVector
Shape::interpolate(RefSCVector& p1,RefSCVector& p2,double val)
{
  if (val < 0.0) {
      failure("Shape::interpolate(): val is < 0.0");
    }
  
  RefSCVector A(p1);
  RefSCVector B(p2);

  set_x(A);
  double value0 = value() - val;

  set_x(B);
  double value1 = value() - val;

  if (value0*value1 > 0.0) {
      failure("Shape::interpolate(): values at endpoints don't bracket val");
    }
  else if (value0 == 0.0) {
      return p1;
    }
  else if (value1 == 0.0) {
      return p2;
    }

  RefSCVector BA = B - A;

  double length = sqrt(BA.scalar_product(BA));
  int niter = (int) (log(length/interpolation_accuracy())/M_LN2);

  double f0 = 0.0;
  double f1 = 1.0;
  double fnext = 0.5;

  RefSCVector X = A + fnext*BA;
  set_x(X);
  double valuenext = value() - val;

  do {
      for (int i=0; i<niter; i++) {
          if (valuenext*value0 <= 0.0) {
              value1 = valuenext;
              f1 = fnext;
              fnext = (f0 + fnext)*0.5;
            }
          else {
              value0 = valuenext;
              f0 = fnext;
              fnext = (fnext + f1)*0.5;
            }
          X = A + fnext*BA;
          set_x(X);
          valuenext = value() - val;
        }
      niter = 1;
    } while (valuenext < 0.0);

  RefSCVector result(dimension());
  result.assign(X);
  return result;
}


//////////////////////////////////////////////////////////////////////
// SphereShape

#define CLASSNAME SphereShape
#define PARENTS public Shape
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
SphereShape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { Shape::_castdown(cd) };
  return do_castdowns(casts,cd);
}

SphereShape::SphereShape(const SCVector3&o,double r):
  _origin(o),
  _radius(r)
{
}

SphereShape::SphereShape(const SphereShape&s):
  _origin(s._origin),
  _radius(s._radius)
{
}

SphereShape::~SphereShape()
{
}

REF_def(SphereShape);
ARRAY_def(RefSphereShape);
SET_def(RefSphereShape);
ARRAYSET_def(RefSphereShape);

double
SphereShape::distance_to_surface(const SCVector3&p,double*grad) const
{
  int i;
  double r2 = 0.0;
  for (i=0; i<3; i++) {
      double tmp = p[i] - _origin[i];
      r2 += tmp*tmp;
    }
  double r = sqrt(r2);
  double d = r - _radius;
  if (d < 0.0) return -1.0;
  if (grad) {
      SCVector3 pv(p);
      SCVector3 o(_origin);
      SCVector3 unit = pv - o;
      unit.normalize();
      for (i=0; i<3; i++) grad[i] = unit[i];
    }
  return d;
}

void SphereShape::print(FILE*fp) const
{
  fprintf(fp,"SphereShape: r = %8.4f o = (%8.4f %8.4f %8.4f)\n",
          radius(),origin()[0],origin()[1],origin()[2]);
}

void
SphereShape::boundingbox(double valuemin, double valuemax,
                         RefSCVector& p1,
                         RefSCVector& p2)
{
  if (valuemax < 0.0) valuemax = 0.0;

  int i;
  for (i=0; i<3; i++) {
      p1[i] = _origin[i] - _radius - valuemax;
      p2[i] = _origin[i] + _radius + valuemax;
    }
}

////////////////////////////////////////////////////////////////////////
// UncappedTorusHoleShape

#define CLASSNAME UncappedTorusHoleShape
#define PARENTS public Shape
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
UncappedTorusHoleShape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { Shape::_castdown(cd) };
  return do_castdowns(casts,cd);
}

UncappedTorusHoleShape::UncappedTorusHoleShape(double r,
                               const SphereShape& s1,
                               const SphereShape& s2):
_r(r),
_s1(s1),
_s2(s2)
{
}

UncappedTorusHoleShape*
UncappedTorusHoleShape::newUncappedTorusHoleShape(double r,
                                                  const SphereShape&s1,
                                                  const SphereShape&s2)
{
  // if the probe sphere fits between the two spheres, then there
  // is no need to include this shape
  SCVector3 A(s1.origin());
  SCVector3 B(s2.origin());
  SCVector3 BA = B - A;
  if (2.0*r <  BA.norm() - s1.radius() - s2.radius()) return 0;

  // check to see if the surface is reentrant
  double rrs1 = r+s1.radius();
  double rrs2 = r+s2.radius();
  SCVector3 R12 = ((SCVector3)s1.origin()) - ((SCVector3)s2.origin());
  double r12 = sqrt(R12.dot(R12));
  if (sqrt(rrs1*rrs1-r*r) + sqrt(rrs2*rrs2-r*r) < r12)
    return new ReentrantUncappedTorusHoleShape(r,s1,s2);

  // otherwise create an ordinary torus hole
  return new NonreentrantUncappedTorusHoleShape(r,s1,s2);
}

// Given a node, finds a sphere in the plane of n and the centers
// of _s1 and _s2 that touches the UncappedTorusHole.  There are two
// candidates, the one closest to n is chosen.
SphereShape
UncappedTorusHoleShape::in_plane_sphere(const SCVector3& n) const
{
  // the center of the sphere is given by the vector equation
  // P = A + a R(AB) + b U(perp),
  // where
  // A is the center of _s1
  // B is the center of _s2
  // R(AB) is the vector from A to B, R(AB) = B - A
  // U(perp) is a unit vect perp to R(AB) and lies in the plane of n, A, and B
  // The unknown scalars, a and b are given by solving the following
  // equations:
  // | P - A | = r(A) + _r, and
  // | P - B | = r(B) + _r,
  // which give
  // | a R(AB) + b U(perp) | = r(A) + _r, and
  // | (a-1) R(AB) + b U(perp) | = r(B) + _r.
  // These further simplify to
  // a^2 r(AB)^2 + b^2 = (r(A)+_r)^2, and
  // (a-1)^2 r(AB)^2 + b^2 = (r(B)+_r)^2.
  // Thus,
  // a = (((r(A)+_r)^2 - (r(B)+_r)^2 )/(2 r(AB)^2)) + 1/2
  // b^2 = (r(A)+r)^2 - a^2 r(AB)^2

  double r_a = _s1.radius();
  double r_b = _s2.radius();
  SCVector3 A = _s1.origin();
  SCVector3 B = _s2.origin();
  SCVector3 N = n;
  SCVector3 R_AB = B - A;
  SCVector3 R_AN = N - A;

  // vector projection of R_AN onto R_AB
  SCVector3 P_AN_AB = R_AB * (R_AN.dot(R_AB)/R_AB.dot(R_AB));
  // the perpendicular vector
  SCVector3 U_perp = R_AN - P_AN_AB;

  // if |U| is tiny, then any vector perp to AB will do
  double u = U_perp.dot(U_perp);
  if (u<1.0e-23) {
      SCVector3 vtry = R_AB;
      vtry[0] += 1.0;
      vtry = vtry - R_AB * (vtry.dot(R_AB)/R_AB.dot(R_AB));
      if (vtry.dot(vtry) < 1.0e-23) {
          vtry = R_AB;
          vtry[1] += 1.0;
          vtry = vtry - R_AB * (vtry.dot(R_AB)/R_AB.dot(R_AB));
        }
      U_perp = vtry;
    }

  U_perp.normalize();
  //printf("A: "); A.print();
  //printf("U_perp: "); U_perp.print();
  //printf("R_AB: "); R_AB.print();

  double r_AB = sqrt(R_AB.dot(R_AB));
  double r_A = _s1.radius();
  double r_B = _s2.radius();

  double r_Ar = r_A + _r;
  double r_Br = r_B + _r;
  double a = ((r_Ar*r_Ar - r_Br*r_Br)/(2.0*r_AB*r_AB)) + 0.5;
  double b = sqrt(r_Ar*r_Ar - a*a*r_AB*r_AB);

  //printf("r_Ar = %f, r_AB = %f\n",r_Ar,r_AB);
  //printf("a = %f, b = %f\n",a,b);

  SCVector3 P = A + a * R_AB + b * U_perp;
  //printf("a*R_AB: "); (a*R_AB).print();
  //printf("b*U_perp: "); (b*U_perp).print();

  SphereShape s(P,_r);

  return s;
}

void
UncappedTorusHoleShape::print(FILE*fp) const
{
  fprintf(fp,"UncappedTorusHoleShape:\n");
  fprintf(fp,"  r = %8.5f\n",_r);
  fprintf(fp,"  s1 = ");
  _s1.print(fp);
  fprintf(fp,"  s2 = ");
  _s2.print(fp);
}

void
UncappedTorusHoleShape::boundingbox(double valuemin, double valuemax,
                                    RefSCVector& p1,
                                    RefSCVector& p2)
{
  RefSCVector p11(dimension());
  RefSCVector p12(dimension());
  RefSCVector p21(dimension());
  RefSCVector p22(dimension());

  _s1.boundingbox(valuemin,valuemax,p11,p12);
  _s2.boundingbox(valuemin,valuemax,p21,p22);

  int i;
  for (i=0; i<3; i++) {
      if (p11[i] < p21[i]) p1[i] = p11[i];
      else p1[i] = p21[i];
      if (p12[i] > p22[i]) p2[i] = p12[i];
      else p2[i] = p22[i];
    }
}

/////////////////////////////////////////////////////////////////////
// is in triangle

static int
is_in_unbounded_triangle(const SCVector3&XP,const SCVector3&AP,const SCVector3&BP)
{
  SCVector3 axis = BP.cross(AP);

  SCVector3 BP_perp = BP; BP_perp.rotate(M_PI_2,axis);
  double u = BP_perp.dot(XP)/BP_perp.dot(AP);
  if (u<0.0) return 0;

  SCVector3 AP_perp = AP; AP_perp.rotate(M_PI_2,axis);
  double w = AP_perp.dot(XP)/AP_perp.dot(BP);
  if (w<0.0) return 0;

  return 1;
}

/////////////////////////////////////////////////////////////////////
// ReentrantUncappedTorusHoleShape

#define CLASSNAME ReentrantUncappedTorusHoleShape
#define PARENTS public UncappedTorusHoleShape
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
ReentrantUncappedTorusHoleShape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { UncappedTorusHoleShape::_castdown(cd) };
  return do_castdowns(casts,cd);
}

ReentrantUncappedTorusHoleShape::ReentrantUncappedTorusHoleShape(double r,
                                                 const SphereShape& s1,
                                                 const SphereShape& s2):
  UncappedTorusHoleShape(r,s1,s2)
{
  rAP = r + s1.radius();
  rBP = r + s2.radius();
  BA = B() - A();

  // Find the points at the ends of the two cone-like objects, I[0] and I[1].
  // they are given by:
  //   I = z BA, where BA = B-A and I is actually IA = I - A
  //   r^2 = PI.PI, where PI = PA-I and P is the center of a probe sphere
  // this gives
  //  z^2 BA.BA - 2z PA.BA + PA.PA - r^2 = 0

  SCVector3 arbitrary; 
  arbitrary[0] = arbitrary[1] = arbitrary[2] = 0.0;
  SphereShape PS = in_plane_sphere(arbitrary);
  SCVector3 P(PS.origin());
  SCVector3 PA = P - A();

  double a = BA.dot(BA);
  double minus_b = 2.0 * PA.dot(BA);
  double c = PA.dot(PA) - r*r;
  double b2m4ac = minus_b*minus_b - 4*a*c;
  double sb2m4ac;
  if (b2m4ac >= 0.0) {
      sb2m4ac = sqrt(b2m4ac);
    }
  else if (b2m4ac > -1.0e-10) {
      sb2m4ac = 0.0;
    }
  else {
      fprintf(stderr,"ReentrantUncappedTorusHoleShape:: imaginary point\n");
      abort();
    }
  double zA = (minus_b - sb2m4ac)/(2.0*a);
  double zB = (minus_b + sb2m4ac)/(2.0*a);
  I[0] = BA * zA + A();
  I[1] = BA * zB + A();
}
ReentrantUncappedTorusHoleShape::~ReentrantUncappedTorusHoleShape()
{
}
int
ReentrantUncappedTorusHoleShape::
  is_outside(const SCVector3&X) const
{
  SCVector3 Xv(X);

  SCVector3 P = in_plane_sphere(X).origin();
  SCVector3 XP = Xv - P;
  double rXP = XP.norm();
  if (rXP > rAP || rXP > rBP) return 1;

  SCVector3 AP = A() - P;
  SCVector3 BP = B() - P;

  if (!is_in_unbounded_triangle(XP,AP,BP)) return 1;

  if (rXP < radius()) {
      return 1;
    }

  return 0;
}
double
ReentrantUncappedTorusHoleShape::
  distance_to_surface(const SCVector3&X,double*grad) const
{
  SCVector3 Xv(X);

  SCVector3 P = in_plane_sphere(X).origin();
  SCVector3 XP = Xv - P;
  double rXP = XP.norm();
  if (rXP > rAP || rXP > rBP) return infinity;

  SCVector3 AP = A() - P;
  SCVector3 BP = B() - P;

  if (!is_in_unbounded_triangle(XP,AP,BP)) return infinity;

  SCVector3 I1P = I[0] - P;
  SCVector3 I2P = I[1] - P;

  if (!is_in_unbounded_triangle(XP,I1P,I2P)) {
      if (rXP < radius()) {
          if (grad) {
              SCVector3 unit(XP);
              unit.normalize();
              for (int i=0; i<3; i++) grad[i] = - unit[i];
            }
          return radius() - rXP;
        }
      else return -1.0;
    }

  return closest_distance(Xv,I,2,grad);
}

/////////////////////////////////////////////////////////////////////
// NonreentrantUncappedTorusHoleShape

#define CLASSNAME NonreentrantUncappedTorusHoleShape
#define PARENTS public UncappedTorusHoleShape
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
NonreentrantUncappedTorusHoleShape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { UncappedTorusHoleShape::_castdown(cd) };
  return do_castdowns(casts,cd);
}

NonreentrantUncappedTorusHoleShape::
  NonreentrantUncappedTorusHoleShape(double r,
                                     const SphereShape& s1,
                                     const SphereShape& s2):
  UncappedTorusHoleShape(r,s1,s2)
{
  rAP = r + s1.radius();
  rBP = r + s2.radius();
  BA = B() - A();
}
NonreentrantUncappedTorusHoleShape::~NonreentrantUncappedTorusHoleShape()
{
}
double NonreentrantUncappedTorusHoleShape::
  distance_to_surface(const SCVector3&X,double* grad) const
{
  SCVector3 Xv(X);

  SphereShape s(in_plane_sphere(X));
  SCVector3 P = s.origin();
  SCVector3 PX = P - Xv;
  double rPX = PX.norm();
  if (rPX > rAP || rPX > rBP) return infinity;

  SCVector3 PA = P - A();
  SCVector3 XA = Xv - A();

  SCVector3 axis = BA.cross(PA);

  SCVector3 BA_perp = BA; BA_perp.rotate(M_PI_2,axis);
  double u = BA_perp.dot(XA)/BA_perp.dot(PA);
  if (u<0.0 || u>1.0) return infinity;

  SCVector3 PA_perp = PA; PA_perp.rotate(M_PI_2,axis);
  double w = PA_perp.dot(XA)/PA_perp.dot(BA);
  if (w<0.0 || w>1.0) return infinity;

  double uw = u+w;
  if (uw<0.0 || uw>1.0) return infinity;

  if (rPX < radius()) {
      if (grad) {
          SCVector3 unit(PX);
          unit.normalize();
          for (int i=0; i<3; i++) grad[i] = unit[i];
        }
      return radius() - rPX;
    }

  return -1;
}

/////////////////////////////////////////////////////////////////////
// Uncapped5SphereExclusionShape

#define CLASSNAME Uncapped5SphereExclusionShape
#define PARENTS public Shape
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
Uncapped5SphereExclusionShape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { Shape::_castdown(cd) };
  return do_castdowns(casts,cd);
}

Uncapped5SphereExclusionShape*
Uncapped5SphereExclusionShape::
  newUncapped5SphereExclusionShape(double r,
                                   const SphereShape& s1,
                                   const SphereShape& s2,
                                   const SphereShape& s3)
{
  Uncapped5SphereExclusionShape* s =
    new Uncapped5SphereExclusionShape(r,s1,s2,s3);
  if (s->solution_exists()) {
      return s;
    }
  delete s;
  return 0;
}
Uncapped5SphereExclusionShape::
  Uncapped5SphereExclusionShape(double radius,
                                const SphereShape&s1,
                                const SphereShape&s2,
                                const SphereShape&s3):
  _r(radius),
  _s1(s1),
  _s2(s2),
  _s3(s3)
{
  double rAr = rA() + r();
  double rAr2 = rAr*rAr;
  double rBr = rB() + r();
  double rBr2 = rBr*rBr;
  double rCr = rC() + r();
  double rCr2 = rCr*rCr;
  double A2 = A().dot(A());
  double B2 = B().dot(B());
  double C2 = C().dot(C());
  SCVector3 BA = B()-A();
  double DdotBA = 0.5*(rAr2 - rBr2 + B2 - A2);
  double DAdotBA = DdotBA - A().dot(BA);
  double BA2 = BA.dot(BA);
  SCVector3 CA = C() - A();
  double CAdotBA = CA.dot(BA);
  SCVector3 CA_perpBA = CA - (CAdotBA/BA2)*BA;
  double CA_perpBA2 = CA_perpBA.dot(CA_perpBA);
  if (CA_perpBA2 < 1.0e-23) {
      _solution_exists = 0;
      return;
    }
  double DdotCA_perpBA = 0.5*(rAr2 - rCr2 + C2 - A2)
    - CAdotBA*DdotBA/BA2;
  double DAdotCA_perpBA = DdotCA_perpBA - A().dot(CA_perpBA);
  double rAt2 = DAdotBA*DAdotBA/BA2 + DAdotCA_perpBA*DAdotCA_perpBA/CA_perpBA2;
  double h2 = rAr2 - rAt2;
  if (h2 <= 0.0) {
      _solution_exists = 0;
      return;
    }
  _solution_exists = 1;

  double h = sqrt(h2);
  if (h<r()) _reentrant = 1;
  else _reentrant = 0;

  // The projection of D into the ABC plane
  M = A() + (DAdotBA/BA2)*BA + (DAdotCA_perpBA/CA_perpBA2)*CA_perpBA;
  SCVector3 BAxCA = BA.cross(CA);
  D[0] = M + h * BAxCA * ( 1.0/BAxCA.norm() );
  D[1] = M - h * BAxCA * ( 1.0/BAxCA.norm() );

  D[0].print();
  D[1].print();
  A().print();
  B().print();
  C().print();

  for (int i=0; i<2; i++) {
      SCVector3 AD = A() - D[i];
      SCVector3 BD = B() - D[i];
      SCVector3 CD = C() - D[i];
      BDxCD[i] = BD.cross(CD);
      BDxCDdotAD[i] = BDxCD[i].dot(AD);
      CDxAD[i] = CD.cross(AD);
      CDxADdotBD[i] = CDxAD[i].dot(BD);
      ADxBD[i] = AD.cross(BD);
      ADxBDdotCD[i] = ADxBD[i].dot(CD);
    }

  // reentrant surfaces need a whole bunch more to be able to compute
  // the distance to the surface
  if (_reentrant) {
      int i;
      for (i=0; i<2; i++) MD[i] = M - D[i];
      double rMD = MD[0].norm(); // this is the same as rMD[1]
      theta_intersect = M_PI_2 - asin(rMD/r());
      r_intersect = r() * sin(theta_intersect);

      {
        // Does the circle of intersection intersect with BA?
        SCVector3 MA = M - A();
        SCVector3 uBA = B() - A(); uBA.normalize();
        SCVector3 XA = uBA * MA.dot(uBA);
        SCVector3 XM = XA - MA;
        double rXM2 = XM.dot(XM);
        double d_intersect_from_x2 = r_intersect*r_intersect - rXM2;
        if (d_intersect_from_x2 > 0.0) {
            _intersects_AB = 1;
            double tmp = sqrt(d_intersect_from_x2);
            double d_intersect_from_x[2];
            d_intersect_from_x[0] = tmp;
            d_intersect_from_x[1] = -tmp;
            for (i=0; i<2; i++) {
                for (int j=0; j<2; j++) {
                    IABD[i][j] = XM + d_intersect_from_x[j]*uBA + MD[i];
                  }
              }
          }
        else _intersects_AB = 0;
      }

      {
        // Does the circle of intersection intersect with BC?
        SCVector3 MC = M - C();
        SCVector3 uBC = B() - C(); uBC.normalize();
        SCVector3 XC = uBC * MC.dot(uBC);
        SCVector3 XM = XC - MC;
        double rXM2 = XM.dot(XM);
        double d_intersect_from_x2 = r_intersect*r_intersect - rXM2;
        if (d_intersect_from_x2 > 0.0) {
            _intersects_BC = 1;
            double tmp = sqrt(d_intersect_from_x2);
            double d_intersect_from_x[2];
            d_intersect_from_x[0] = tmp;
            d_intersect_from_x[1] = -tmp;
            for (i=0; i<2; i++) {
                for (int j=0; j<2; j++) {
                    IBCD[i][j] = XM + d_intersect_from_x[j]*uBC + MD[i];
                  }
              }
          }
        else _intersects_BC = 0;
      }

      {
        // Does the circle of intersection intersect with CA?
        SCVector3 MA = M - A();
        SCVector3 uCA = C() - A(); uCA.normalize();
        SCVector3 XA = uCA * MA.dot(uCA);
        SCVector3 XM = XA - MA;
        double rXM2 = XM.dot(XM);
        double d_intersect_from_x2 = r_intersect*r_intersect - rXM2;
        if (d_intersect_from_x2 > 0.0) {
            _intersects_CA = 1;
            double tmp = sqrt(d_intersect_from_x2);
            double d_intersect_from_x[2];
            d_intersect_from_x[0] = tmp;
            d_intersect_from_x[1] = -tmp;
            for (i=0; i<2; i++) {
                for (int j=0; j<2; j++) {
                    ICAD[i][j] = XM + d_intersect_from_x[j]*uCA + MD[i];
                  }
              }
          }
        else _intersects_CA = 0;
      }

    }
}
int
Uncapped5SphereExclusionShape::is_outside(const SCVector3&X) const
{
  SCVector3 Xv(X);

  for (int i=0; i<2; i++) {
      SCVector3 XD = Xv - D[i];
      double rXD = XD.norm();
      if (rXD <= r()) return 1;
      double u = BDxCD[i].dot(XD)/BDxCDdotAD[i];
      if (u <= 0.0) return 1;
      double v = CDxAD[i].dot(XD)/CDxADdotBD[i];
      if (v <= 0.0) return 1;
      double w = ADxBD[i].dot(XD)/ADxBDdotCD[i];
      if (w <= 0.0) return 1;
    }

  return 0;
}
static int
is_contained_in_unbounded_pyramid(SCVector3 XD,
                                  SCVector3 AD,
                                  SCVector3 BD,
                                  SCVector3 CD)
{
  SCVector3 BDxCD = BD.cross(CD);
  SCVector3 CDxAD = CD.cross(AD);
  SCVector3 ADxBD = AD.cross(BD);
  double u = BDxCD.dot(XD)/BDxCD.dot(AD);
  if (u <= 0.0) return 0;
  double v = CDxAD.dot(XD)/CDxAD.dot(BD);
  if (v <= 0.0) return 0;
  double w = ADxBD.dot(XD)/ADxBD.dot(CD);
  if (w <= 0.0) return 0;
  return 1;
}
double
Uncapped5SphereExclusionShape::
  distance_to_surface(const SCVector3&X,double*grad) const
{
  SCVector3 Xv(X);

  for (int i=0; i<2; i++) {
      SCVector3 XD = Xv - D[i];
      double u = BDxCD[i].dot(XD)/BDxCDdotAD[i];
      if (u <= 0.0) return infinity;
      double v = CDxAD[i].dot(XD)/CDxADdotBD[i];
      if (v <= 0.0) return infinity;
      double w = ADxBD[i].dot(XD)/ADxBDdotCD[i];
      if (w <= 0.0) return infinity;
      double rXD = XD.norm();
      if (rXD <= r()) {
          if (!_reentrant) return r() - rXD;
          // this shape is reentrant
          // make sure that we're on the right side
          if ((i == 1) || (u + v + w <= 1.0)) {
              // see if we're outside the cone that intersects
              // the opposite sphere
              double angle = acos(fabs(XD.dot(MD[i]))
                                  /(XD.norm()*MD[i].norm()));
              if (angle >= theta_intersect) {
                  if (grad) {
                      for (int j=0; j<3; j++) grad[j] = -XD[j]/rXD;
                    }
                  return r() - rXD;
                }
              if (_intersects_AB
                  &&is_contained_in_unbounded_pyramid(XD,
                                                      MD[i],
                                                      IABD[i][0],
                                                      IABD[i][1])) {
                  //printf("XD: "); XD.print();
                  //printf("MD[%d]: ",i); MD[i].print();
                  //printf("IABD[%d][0]: ",i); IABD[i][0].print();
                  //printf("IABD[%d][1]: ",i); IABD[i][1].print();
                  return closest_distance(XD,IABD[i],2,grad);
                }
              if (_intersects_BC
                  &&is_contained_in_unbounded_pyramid(XD,
                                                      MD[i],
                                                      IBCD[i][0],
                                                      IBCD[i][1])) {
                  return closest_distance(XD,IBCD[i],2,grad);
                }
              if (_intersects_CA
                  &&is_contained_in_unbounded_pyramid(XD,
                                                      MD[i],
                                                      ICAD[i][0],
                                                      ICAD[i][1])) {
                  return closest_distance(XD,ICAD[i],2,grad);
                }
              // at this point we are closest to the ring formed
              // by the intersection of the two probe spheres
              double distance_to_plane;
              double distance_to_ring_in_plane;
              double MDnorm = MD[i].norm();
              SCVector3 XM = XD - MD[i];
              SCVector3 XM_in_plane;
              if (MDnorm<1.0e-16) {
                  distance_to_plane = 0.0;
                  XM_in_plane = XD;
                }
              else {
                  distance_to_plane = XM.dot(MD[i])/MDnorm;
                  XM_in_plane = XM - (distance_to_plane/MDnorm)*MD[i];
                }
              if (grad) {
                  double XM_in_plane_norm = XM_in_plane.norm();
                  if (XM_in_plane_norm < 1.e-8) {
                      // the grad points along MD
                      double scale = - distance_to_plane
                        /(MDnorm*sqrt(r_intersect*r_intersect
                                      + distance_to_plane*distance_to_plane));
                      for (int j=0; j<3; j++) grad[j] = MD[i][j] * scale;
                    }
                  else {
                      SCVector3 point_on_ring;
                      point_on_ring = XM_in_plane
                        * (r_intersect/XM_in_plane_norm) + M;
                      SCVector3 gradv = Xv - point_on_ring;
                      gradv.normalize();
                      for (int j=0; j<3; j++) grad[j] = gradv[j];
                    }
                }
              distance_to_ring_in_plane =
                r_intersect - sqrt(XM_in_plane.dot(XM_in_plane));
              return sqrt(distance_to_ring_in_plane*distance_to_ring_in_plane
                          +distance_to_plane*distance_to_plane);
            }
        }
    }

  return -1.0;
}

void
Uncapped5SphereExclusionShape::boundingbox(double valuemin, double valuemax,
                                           RefSCVector& p1,
                                           RefSCVector& p2)
{
  RefSCVector p11;
  RefSCVector p12;
  RefSCVector p21;
  RefSCVector p22;
  RefSCVector p31;
  RefSCVector p32;

  _s1.boundingbox(valuemin,valuemax,p11,p12);
  _s2.boundingbox(valuemin,valuemax,p21,p22);
  _s3.boundingbox(valuemin,valuemax,p31,p32);

  int i;
  for (i=0; i<3; i++) {
      if (p11[i] < p21[i]) p1[i] = p11[i];
      else p1[i] = p21[i];
      if (p31[i] < p1[i]) p1[i] = p31[i];
      if (p12[i] > p22[i]) p2[i] = p12[i];
      else p2[i] = p22[i];
      if (p32[i] > p2[i]) p2[i] = p32[i];
    }
}

/////////////////////////////////////////////////////////////////////
// Unionshape

#define CLASSNAME UnionShape
#define PARENTS public Shape
#include <util/state/statei.h>
#include <util/class/classia.h>
void *
UnionShape::_castdown(const ClassDesc*cd)
{
  void* casts[] =  { Shape::_castdown(cd) };
  return do_castdowns(casts,cd);
}

UnionShape::UnionShape()
{
}

UnionShape::~UnionShape()
{
}

void
UnionShape::add_shape(RefShape s)
{
  _shapes.add(s);
}

double
UnionShape::distance_to_surface(const SCVector3&p,double* grad) const
{
  if (_shapes.length() == 0) return 0.0;
  double min = _shapes[0]->distance_to_surface(p);
  int imin = 0;
  if (min < 0.0) return -1.0;
  for (int i=1; i<_shapes.length(); i++) {
      double d = _shapes[i]->distance_to_surface(p);
      if (d < 0.0) return -1.0;
      if (min > d) { min = d; imin = i; }
    }

  if (grad) {
      _shapes[imin]->distance_to_surface(p,grad);
    }
  return min;
}

int
UnionShape::is_outside(const SCVector3&p) const
{
  if (_shapes.length() == 0) return 1;
  for (int i=0; i<_shapes.length(); i++) {
      if (!_shapes[i]->is_outside(p)) return 0;
    }

  return 1;
}

void
UnionShape::boundingbox(double valuemin, double valuemax,
                        RefSCVector& p1,
                        RefSCVector& p2)
{
  if (_shapes.length() == 0) {
      for (int i=0; i<3; i++) p1[i] = p2[i] = 0.0;
      return;
    }
  
  RefSCVector pt1(dimension());
  RefSCVector pt2(dimension());
  
  int i,j;
  _shapes[0]->boundingbox(valuemin,valuemax,p1,p2);
  for (j=1; j<_shapes.length(); j++) {
      _shapes[j]->boundingbox(valuemin,valuemax,pt1,pt2);
      for (i=0; i<3; i++) {
          if (pt1[i] < p1[i]) p1[i] = pt1[i];
          if (pt2[i] > p2[i]) p2[i] = pt2[i];
        }
    }
}
