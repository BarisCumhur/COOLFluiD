#include "STKT_LDACSchemeScalar.hh"
#include "Common/NotImplementedException.hh"
#include "Framework/CFL.hh"
#include "Framework/SubSystemStatus.hh"
#include "FluctSplit/FluctSplitSpaceTime.hh"
#include "Framework/MethodStrategyProvider.hh"
#include "FluctSplit/FluctuationSplitData.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::MathTools;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {



    namespace FluctSplit {

//////////////////////////////////////////////////////////////////////////////

MethodStrategyProvider<STKT_LDACSchemeScalar,
		       FluctuationSplitData,
		       Splitter,
                       FluctSplitSpaceTimeModule>
spaceTimeRiccLDAcSchemeScalarProvider("STKT_ScalarLDAC");

//////////////////////////////////////////////////////////////////////////////

STKT_LDACSchemeScalar::STKT_LDACSchemeScalar(const std::string& name) :
  STKT_SplitterScalar(name),
  m_sumKplus(),
  m_uTemp(),
  m_phiT()
{
}

//////////////////////////////////////////////////////////////////////////////

STKT_LDACSchemeScalar::~STKT_LDACSchemeScalar()
{
}

//////////////////////////////////////////////////////////////////////////////

void STKT_LDACSchemeScalar::setup()
{
  STKT_SplitterScalar::setup();
 
  // Do not need a dissipation contribution from past 
 getMethodData().getDistributionData().needDiss = false;

  m_sumKplus.resize(_nbEquations);
  m_uTemp.resize(_nbEquations); 
  m_phiT.resize(_nbEquations);
 
 
}

//////////////////////////////////////////////////////////////////////////////

void STKT_LDACSchemeScalar::computePicardJacob(std::vector<RealMatrix*>& jacob)
{
  throw Common::NotImplementedException (FromHere(),"SpaceTimeLDASchemeScalar::computePicardJacob()");
}

//////////////////////////////////////////////////////////////////////////////

void STKT_LDACSchemeScalar::distribute(vector<RealVector>& residual)
{  
   ///@todo No mesh deformation implemented here!!
  const CFuint nbStatesInCell = _nbStatesInCell;
  DistributionData& ddata = getMethodData().getDistributionData();
  m_phiT = ddata.phi;

  m_sumKplus = _kPlus[0];
  for (CFuint iState = 1; iState < nbStatesInCell; ++ iState){
    m_sumKplus += _kPlus[iState];
  }

  for (CFuint iState = 0; iState < nbStatesInCell; ++ iState){ 
    m_uTemp = _kPlus[iState]/m_sumKplus;

   residual[iState] = m_uTemp*m_phiT;
 
}

  vector<RealMatrix>& betas = 
      *ddata.currBetaMat;
    for (CFuint iState = 0; iState < _nbStatesInCell; ++iState) {
      // AL: compute betas on the fly is more expensive because
      // it involves a matrix*matrix and a matrix*vector
      // while normal scheme requires only two matrix*vector operations
      
      betas[iState] = _kPlus[iState]/m_sumKplus;
    }
}

//////////////////////////////////////////////////////////////////////////////

     } // namespace FluctSplit



} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
