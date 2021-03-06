#ifndef COOLFluiD_SpectralFV_DivideByVolumeRHSSpectralFV_hh
#define COOLFluiD_SpectralFV_DivideByVolumeRHSSpectralFV_hh

//////////////////////////////////////////////////////////////////////////////

#include "Framework/DataSocketSink.hh"

#include "SpectralFV/BaseVolTermComputer.hh"
#include "SpectralFV/SpectralFVMethodData.hh"

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace SpectralFV {

//////////////////////////////////////////////////////////////////////////////

/**
 * This class represent a command that multiplies the residuals with the inverse volumes
 *
 * @author Kris Van den Abeele
 *
 */
class DivideByVolumeRHSSpectralFV : public SpectralFVMethodCom {
public:

  /**
   * Constructor.
   */
  explicit DivideByVolumeRHSSpectralFV(const std::string& name);

  /**
   * Destructor.
   */
  virtual ~DivideByVolumeRHSSpectralFV();

  /**
   * Setup private data and data of the aggregated classes
   * in this command before processing phase
   */
  virtual void setup();

  /**
   * Unsetup private data
   */
  virtual void unsetup();

  /**
   * Configures the command.
   */
  virtual void configure ( Config::ConfigArgs& args );

  /**
   * Execute Processing actions
   */
  virtual void execute();

  /**
   * Returns the DataSocket's that this command needs as sinks
   * @return a vector of SafePtr with the DataSockets
   */
  virtual std::vector<Common::SafePtr<Framework::BaseDataSocketSink> > needsSockets();

protected: // functions

  /**
   * Divides the rhs by the cell volume
   */
  void divideRHSByVolume();

protected: // data

  /// storage of the rhs
  Framework::DataSocketSink<CFreal> socket_rhs;

  /// storage of the rhs
  Framework::DataSocketSink<CFreal> socket_updateCoeff;

  /// builder of cells
  Common::SafePtr<Framework::GeometricEntityPool<Framework::StdTrsGeoBuilder> > m_cellBuilder;

  /// inverse volume fractions of CVs
  Common::SafePtr< std::vector< CFreal > > m_invVolFracCVs;

  /// variable for cell
  Framework::GeometricEntity* m_cell;

  /// vector containing pointers to the states in a cell
  std::vector< Framework::State* >* m_cellStates;

  /// number of equations in the physical model
  CFuint m_nbrEqs;

}; // class DivideByVolumeRHSSpectralFV

//////////////////////////////////////////////////////////////////////////////

  } // namespace SpectralFV

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

#endif // COOLFluiD_SpectralFV_DivideByVolumeRHSSpectralFV_hh
