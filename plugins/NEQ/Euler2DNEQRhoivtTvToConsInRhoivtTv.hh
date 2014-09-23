#ifndef COOLFluiD_Physics_NEQ_Euler2DNEQRhoivtTvToConsInRhoivtTv_hh
#define COOLFluiD_Physics_NEQ_Euler2DNEQRhoivtTvToConsInRhoivtTv_hh

//////////////////////////////////////////////////////////////////////////////

#include "Framework/VarSetMatrixTransformer.hh"
#include "Framework/MultiScalarTerm.hh"
#include "NavierStokes/EulerTerm.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Physics {

    namespace NEQ {

//////////////////////////////////////////////////////////////////////////////

/**
 * This class represents a transformer of variables from conservative
 * to Puvt to consistent conservative variables
 *
 * @author Andrea Lani
 *
 */
class Euler2DNEQRhoivtTvToConsInRhoivtTv : public Framework::VarSetMatrixTransformer {
public:
  
  typedef Framework::MultiScalarTerm<NavierStokes::EulerTerm> NEQTerm;
  
  /**
   * Default constructor without arguments
   */
  Euler2DNEQRhoivtTvToConsInRhoivtTv(Common::SafePtr<Framework::PhysicalModelImpl> model);

  /**
   * Default destructor
   */
  ~Euler2DNEQRhoivtTvToConsInRhoivtTv();

 /**
   * Set the transformation matrix from a given state
   */
  void setMatrix(const RealVector& state);

private:

  /**
   * Set the flag telling if the transformation is an identity one
   * @pre this method must be called during set up
   */
  bool getIsIdentityTransformation() const
  {
    return false;
  }
  
private: //data
  
  /// acquaintance of the model
  Common::SafePtr<NEQTerm> _model;
  
  /// Vector storing the elemental composition
  RealVector _ys;
  
  /// array with all different vibrational dimensional temperatures
  RealVector _tvDim;
  
  /// array with all different vibrational dimensional energies
  RealVector _evDim;  
  
  /// array to store density, enthalpy and energy
  RealVector _dhe;
  
}; // end of class Euler2DNEQRhoivtTvToConsInRhoivtTv

//////////////////////////////////////////////////////////////////////////////

    } // namespace NEQ

  } // namespace Physics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Physics_NEQ_Euler2DNEQRhoivtTvToConsInRhoivtTv_hh
