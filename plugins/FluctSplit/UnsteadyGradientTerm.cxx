#include "Environment/ObjectProvider.hh"

#include "Common/BadValueException.hh"
#include "Framework/GeometricEntity.hh"
#include "Framework/MeshData.hh"
#include "Framework/MethodStrategyProvider.hh"
#include "LESvki/Gradient2DVarSet.hh"
#include "Framework/SubSystemStatus.hh"

#include "FluctSplit/FluctSplitLES.hh"
#include "FluctSplit/UnsteadyGradientTerm.hh"
#include "FluctSplit/InwardNormalsData.hh"

#include "NavierStokes/EulerVarSet.hh"
//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Physics::NavierStokes;
using namespace COOLFluiD::Physics::LESvki;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

    namespace FluctSplit {

//////////////////////////////////////////////////////////////////////////////

MethodStrategyProvider<UnsteadyGradientTerm,
		       FluctuationSplitData,
		       ComputeDiffusiveTerm,
		       FluctSplitLESModule>
UnsteadyGradient2dDiffusiveTermProvider("UnsteadyGradient2d");

//////////////////////////////////////////////////////////////////////////////

UnsteadyGradientTerm::UnsteadyGradientTerm(const std::string& name) :
  ComputeDiffusiveTerm(name),
  _diffVar(CFNULL),
  _updateVar(CFNULL),
  _radius(0.),
  _states(),
  _values(),
  _avValues(CFNULL),
  _avState(CFNULL),
  _normal(), 
  _gradRho(),
  _gradRhoU(),
  _gradRhoV(),
  _edata(),
  socket_pastStates("pastStates")
{
}

//////////////////////////////////////////////////////////////////////////////

UnsteadyGradientTerm::~UnsteadyGradientTerm()
{
  deletePtr(_avValues);
  deletePtr(_avState);
}

//////////////////////////////////////////////////////////////////////////////

void UnsteadyGradientTerm::setDiffusiveVarSet(SafePtr<DiffusiveVarSet> diffVar)
{
  _diffVar = diffVar.d_castTo<Gradient2DVarSet>();
 }

//////////////////////////////////////////////////////////////////////////////

void UnsteadyGradientTerm::setUpdateVarSet(SafePtr<ConvectiveVarSet> updateVar)
{
  _updateVar = updateVar.d_castTo<EulerVarSet>();
}

//////////////////////////////////////////////////////////////////////////////

void UnsteadyGradientTerm::computeDiffusiveTerm
(GeometricEntity *const geo, vector<RealVector>& result, bool updateCoeffFlag)
{
  DistributionData& ddata = getMethodData().getDistributionData();
  const CFuint nbStatesInCell = ddata.states->size();
  const CFreal ovNbStatesInCell = 1./nbStatesInCell;
  const CFuint nbEqs = PhysicalModelStack::getActive()->getNbEq();
  
  // Start with the part from the past
  DataHandle<State*> pastStatesStorage = socket_pastStates.getDataHandle();
  // get the paststates in this cell
  for (CFuint i = 0; i < nbStatesInCell; ++i) {
    const CFuint stateID = (*ddata.states)[i]->getLocalID();
    _states[i] = pastStatesStorage[stateID];
  }
  
  // Computing an average on the cell
  computeAverageState(nbStatesInCell, ovNbStatesInCell, nbEqs, _states, *_avState);
  this->getMethodData().getDistributionData().avState = *_avState;
  getMethodData().getUpdateVar()->computePhysicalData(*_avState, _edata);
  computeCellGradientsAndAverageState(geo, _edata);
  
  DataHandle< InwardNormalsData*> normals = socket_normals.getDataHandle();
  DataHandle< CFreal> updateCoeff = socket_updateCoeff.getDataHandle();
  
  const CFuint nbCellStates = geo->getStates()->size();
  const CFuint cellID = geo->getID();
  const CFuint dim = PhysicalModelStack::getActive()->getDim();
  const CFreal dtcoeff = SubSystemStatusStack::getActive()->getDT()/(2.0*dim);
  NSTerm& model = _diffVar->getModel();
  vector<RealVector*>& gradients = ddata.gradients;
  
  // set the diffusive term
  for (CFuint i = 0; i < nbCellStates; ++i) {
    // this is not the unit normal !!
    for (CFuint iDim = 0; iDim < dim; ++iDim) {
      _normal[iDim] = normals[cellID]->getNodalNormComp(i,iDim);
    }
    const RealVector& flux = _diffVar->getFlux(*_avState, gradients, _normal,sqrt(geo->computeVolume()));
    result[i] = (-dtcoeff)*flux;
  }
  
  // Now contribution from the present
  // store the pointers to state in another array (of RealVector*)
  vector<State*> *const cellStates = geo->getStates();
  for (CFuint i = 0; i < nbStatesInCell; ++i) {
    _states[i] = (*cellStates)[i];
  }
  
  computeAverageState(nbStatesInCell, ovNbStatesInCell, nbEqs, _states, *_avState);
  this->getMethodData().getDistributionData().avState = *_avState;
  getMethodData().getUpdateVar()->computePhysicalData(*_avState, _edata);
  computeCellGradientsAndAverageState(geo, _edata);
  
  const CFreal dcoeff = (updateCoeffFlag) ? (model.getPhysicalData())[NSTerm::MU]/
    (_edata[EulerTerm::RHO]*this->socket_volumes.getDataHandle()[cellID]*dim*dim) : 0.0;
  
  // set the diffusive term
  for (CFuint i = 0; i < nbCellStates; ++i) {
    // this is not the unit normal !!
    for (CFuint iDim = 0; iDim < dim; ++iDim) {
      _normal[iDim] = normals[cellID]->getNodalNormComp(i,iDim);
    }
    
    const RealVector& flux = _diffVar->getFlux(*_avState, gradients, _normal, sqrt(geo->computeVolume()));
    result[i] += (-dtcoeff)*flux;
    
    if (updateCoeffFlag) {
      const CFreal faceArea = normals[cellID]->getAreaNode(i);
      updateCoeff[geo->getState(i)->getLocalID()] += faceArea*faceArea*dcoeff;
    }
  }
}
      
//////////////////////////////////////////////////////////////////////////////

void UnsteadyGradientTerm::setup()
{
  const CFuint nbEqs = PhysicalModelStack::getActive()->getNbEq();
  const CFuint dim = PhysicalModelStack::getActive()->getDim();
  const CFuint nbNodesInControlVolume =
    MeshDataStack::getActive()->Statistics().getMaxNbStatesInCell();
  
  _states.resize(nbNodesInControlVolume);
  _values.resize(nbEqs, nbNodesInControlVolume);
  _normal.resize(dim); 
  _gradRho.resize(dim);
  _gradRhoU.resize(dim);
  _gradRhoV.resize(dim);
  
  PhysicalModelStack::getActive()->getImplementor()->getConvectiveTerm()->
    resizePhysicalData(_edata); 
  _avValues = new State();
  _avState = new State();
}

//////////////////////////////////////////////////////////////////////////////

void UnsteadyGradientTerm::configure ( Config::ConfigArgs& args )
{
  ComputeDiffusiveTerm::configure(args);
}

//////////////////////////////////////////////////////////////////////////////

void UnsteadyGradientTerm::computeCellGradientsAndAverageState
(GeometricEntity *const geo, const RealVector& pdata)
{
  DataHandle< InwardNormalsData*> normals = socket_normals.getDataHandle();
  const CFuint nbCellStates = _states.size();

  // compute vars that will be used to compute the gradients
  _diffVar->setGradientVars(_states, _values, geo->nbNodes());

  const CFuint cellID = geo->getID();
  const CFreal cellVolume = this->socket_volumes.getDataHandle()[cellID];
  const CFuint dim = PhysicalModelStack::getActive()->getDim();
  const CFreal dimCoeff = 1./dim;
  const CFreal coeffGrad = dimCoeff/cellVolume;
  const CFuint nbEqs = PhysicalModelStack::getActive()->getNbEq();

  vector<RealVector*>& gradients =
    getMethodData().getDistributionData().gradients;

  for (CFuint iEq = 0; iEq < nbEqs; ++iEq) {
    RealVector& grad = *gradients[iEq];
    grad = 0.0;
    
    for (CFuint is = 0; is < nbCellStates; ++is) {
      for (CFuint iDim = 0; iDim < dim; ++iDim) {
	grad[iDim] += _values(iEq,is)*normals[cellID]->getNodalNormComp(is,iDim);
      }
    }
    grad *= coeffGrad;
  }
  
  // new velocity gradients
  _gradRho = 0.0;
  
  for (CFuint is = 0; is < nbCellStates; ++is) {
    for (CFuint iDim = 0; iDim < dim; ++iDim) {
      _gradRho[iDim] += (*_states[is])[0]*normals[cellID]->getNodalNormComp(is,iDim);
    }
  }
  _gradRho *= coeffGrad;
  
  _gradRhoU = 0.0;
  _gradRhoV = 0.0;
  const CFreal avRho = pdata[EulerTerm::RHO];
  const CFreal avU = pdata[EulerTerm::VX];
  const CFreal avV = pdata[EulerTerm::VY];
  
  for (CFuint is = 0; is < nbCellStates; ++is) {
    for (CFuint iDim = 0; iDim < dim; ++iDim) {    
      _gradRhoU[iDim] += (*_states[is])[1]*normals[cellID]->getNodalNormComp(is,iDim);
      _gradRhoV[iDim] += (*_states[is])[2]*normals[cellID]->getNodalNormComp(is,iDim);	
    }
  }
  _gradRhoU *= coeffGrad;
  _gradRhoV *= coeffGrad;
  
  RealVector& gradU = *gradients[1];
  RealVector& gradV = *gradients[2];    
  gradU = (_gradRhoU-avU*_gradRho)/avRho;
  gradV = (_gradRhoV-avV*_gradRho)/avRho;
  
  SafePtr<EulerTerm> eulerModel = _updateVar->getModel();
  if(avRho < 0.0) {
    cout << "negative avRho = " << avRho << " in cell "<< geo->getID() << endl;
    cout << "pdata = " << pdata << endl;
    throw BadValueException (FromHere(),"Negative average density in cell");
  }

  if (pdata[EulerTerm::A] < 0.0) {
    cout << "negative a = " << pdata[EulerTerm::A] << " in cell "<< geo->getID() << endl;
    cout << "pdata = " << pdata << endl;
    throw BadValueException (FromHere(),"Negative average sound speed in cell");
  }
}

//////////////////////////////////////////////////////////////////////////////

} // namespace FluctSplit

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
