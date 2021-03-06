#ifndef COOLFluiD_Numerics_FluctSplit_STKT_SplitterSysScalar_hh
#define COOLFluiD_Numerics_FluctSplit_STKT_SplitterSysScalar_hh

//////////////////////////////////////////////////////////////////////////////

#include "STKT_SplitterSys.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

    namespace FluctSplit {

//////////////////////////////////////////////////////////////////////////////

/// This class represents a generic interface for RDS splitters for
/// system schemes
/// @author Andrea Lani
class STKT_SplitterSysScalar : public STKT_SplitterSys {
  
public:
  
  /// Defines the Config Option's of this class
  /// @param options a OptionList where to add the Option's
  static void defineConfigOptions(Config::OptionList& options);
  
  /// Constructor
  /// @see Splitter()
  STKT_SplitterSysScalar(const std::string& name);
  
  /// Default destructor
  virtual ~STKT_SplitterSysScalar();

  /// Set up
  virtual void setup();
  
  /**
   * Compute the inflow parameters of a space-time element : k_space*dt/DIM + area/(DIM+1)
   * @post K+ and K- will be computed
   */
  virtual void computeK(const std::vector<Framework::State*>& states,
			const InwardNormalsData* const normalsData);
  
private:
  
  /// Sets the correct block limits for a System Splitter
  /// Called by the STKT_Splitter constructor.
  /// @see _nbEquations
  /// @see _firstVarID
  /// @see _lastVarID
  virtual void setBlockData();
  
protected: // data
  
  /// temporary data for computation of upwind parameters
  std::vector<RealVector> _kPlusScalar;

  /// temporary data for computation of upwind parameters
  std::vector<RealVector> _kMinScalar;

  /// temporary data for computation of upwind parameters
  std::vector<RealVector> _kScalar;
  
  /// Size of the coupled system of equations
  CFuint _sysSize;
  
  /// ID corresponding to the start of the system
  CFuint _sysStartID;
  
  /// IDs corresponding to the decoupled scalar equations
  std::vector<CFuint> _scalarEqIDs;
  
}; // end of class STKT_SplitterSysScalar

//////////////////////////////////////////////////////////////////////////////

    } // namespace FluctSplit

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_Numerics_FluctSplit_STKT_SplitterSysScalar_hh
