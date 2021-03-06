#include "FiniteVolume/FiniteVolume.hh"
#include "SteadySilverMullerCondition2D.hh"
#include "Framework/MethodCommandProvider.hh"
#include "Framework/SubSystemStatus.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace FiniteVolume {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<SteadySilverMullerCondition2D, CellCenterFVMData, FiniteVolumeModule> steadySilverMullerCondition2DFVMCCProvider("SteadySilverMullerCondition2DFVMCC");

//////////////////////////////////////////////////////////////////////////////

SteadySilverMullerCondition2D::SteadySilverMullerCondition2D(const std::string& name) :
  SuperInlet(name),
  _variables()
{
}

//////////////////////////////////////////////////////////////////////////////

SteadySilverMullerCondition2D::~SteadySilverMullerCondition2D()
{
}

//////////////////////////////////////////////////////////////////////////////

void SteadySilverMullerCondition2D::setup()
{

  SuperInlet::setup();

  _variables.resize(PhysicalModelStack::getActive()->getDim());

}

//////////////////////////////////////////////////////////////////////////////

void SteadySilverMullerCondition2D::unsetup()
{
  SuperInlet::unsetup();
}

//////////////////////////////////////////////////////////////////////////////

void SteadySilverMullerCondition2D::configure ( Config::ConfigArgs& args )
{
  SuperInlet::configure(args);

}

//////////////////////////////////////////////////////////////////////////////

void SteadySilverMullerCondition2D::setGhostState(GeometricEntity *const face)
{
  State *const innerState = face->getState(0);
  State *const ghostState = face->getState(1);
  
   const CFuint faceID = face->getID();
   const CFuint startID = faceID*PhysicalModelStack::getActive()->getDim();
   
   DataHandle<CFreal> normals = socket_normals.getDataHandle();
   
   CFreal nx = normals[startID];
   CFreal ny = normals[startID + 1];
   CFreal nz = 0; 
   const CFreal invFaceLength = 1./sqrt(nx*nx + ny*ny + nz*nz);
   nx *= invFaceLength;
   ny *= invFaceLength;
   nz *= invFaceLength;
   
   
   

  // coordinate of the boundary point
  _bCoord = (innerState->getCoordinates() +
             ghostState->getCoordinates());
  _bCoord *= 0.5;

  const CFuint nbDim = PhysicalModelStack::getActive()->getDim();
  for(CFuint iDim = 0; iDim < nbDim; iDim++)
  {
    _variables[iDim] = _bCoord[iDim];
  }
//  _variables[PhysicalModelStack::getActive()->getDim()] = SubSystemStatusStack::getActive()->getCurrentTimeDim();

  //Evaluate the function
  _vFunction.evaluate(_variables,*_input);
  
/// Silver Muller Condition Consistent implementation
  
//   //Set the state value
//   if (_inputAdimensionalValues){
//     *ghostState = *_inputToUpdateVar->transform(_input);          
//   }
//   else{
//     *_dimState = *_inputToUpdateVar->transform(_input);
//     _varSet->setAdimensionalValues(*_dimState, *ghostState);   
//     
//   }  
//   
  
  
/// Silver Muller Condition Implementation working in Full Planar wave
  //Set the state value
  if (_inputAdimensionalValues){
    *ghostState = *_inputToUpdateVar->transform(_input);          
  }
  else{
    *_dimState = *_inputToUpdateVar->transform(_input);
    _varSet->setAdimensionalValues(*_dimState, *ghostState);   
    
  }
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace FiniteVolume

  } // namespace Numerics

} // namespace COOLFluiD
