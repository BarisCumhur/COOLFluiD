#include "FiniteVolumeNavierStokes/FiniteVolumeNavierStokes.hh"
#include "FiniteVolumeNavierStokes/UnsteadyFarFieldEuler2D.hh"
#include "Framework/MethodCommandProvider.hh"
#include "NavierStokes/Euler2DVarSet.hh"
#include "Framework/MeshData.hh"
#include "Framework/SubSystemStatus.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Physics::NavierStokes;
using namespace COOLFluiD::MathTools;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace FiniteVolume {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<UnsteadyFarFieldEuler2D, CellCenterFVMData, FiniteVolumeNavierStokesModule> UnsteadyFarFieldEuler2DFVMCCProvider("UnsteadyFarFieldEuler2DFVMCC");

//////////////////////////////////////////////////////////////////////////////

void UnsteadyFarFieldEuler2D::defineConfigOptions(Config::OptionList& options)
{
   options.addConfigOption< CFreal >("Uinf","x velocity");
   options.addConfigOption< CFreal >("Vinf","y velocity");
   options.addConfigOption< CFreal >("Pinf","static pressure");
   options.addConfigOption< CFreal >("Tinf","static temperature");
}

//////////////////////////////////////////////////////////////////////////////

UnsteadyFarFieldEuler2D::UnsteadyFarFieldEuler2D(const std::string& name) :
  FVMCC_BC(name),
  _varSet(CFNULL),
  _dataInnerState(),
  _dataGhostState(),
  socket_pastNodes("pastNodes"),
  socket_futureNodes("futureNodes"),
  _coord(),
  _speed()
{
   addConfigOptionsTo(this);
  _temperature = 0.0;
   setParameter("Tinf",&_temperature);

  _pressure = 0.0;
   setParameter("Pinf",&_pressure);

  _uInf = 0.0;
   setParameter("Uinf",&_uInf);

  _vInf = 0.0;
   setParameter("Vinf",&_vInf);
}

//////////////////////////////////////////////////////////////////////////////

UnsteadyFarFieldEuler2D::~UnsteadyFarFieldEuler2D()
{
}

//////////////////////////////////////////////////////////////////////////////

void UnsteadyFarFieldEuler2D::setup()
{

  FVMCC_BC::setup();

  _coord.resize(PhysicalModelStack::getActive()->getDim());
  _speed.resize(PhysicalModelStack::getActive()->getDim());

  _varSet = getMethodData().getUpdateVar().d_castTo<Euler2DVarSet>();
  _varSet->getModel()->resizePhysicalData(_dataInnerState);
  _varSet->getModel()->resizePhysicalData(_dataGhostState);

  _pressure/=_varSet->getModel()->getPressRef();
  _temperature/=_varSet->getModel()->getTempRef();
  _uInf /= _varSet->getModel()->getVelRef();
  _vInf /= _varSet->getModel()->getVelRef();
}

//////////////////////////////////////////////////////////////////////////////

void UnsteadyFarFieldEuler2D::setGhostState(GeometricEntity *const face)
 {
   State *const innerState = face->getState(0);
   State *const ghostState = face->getState(1);

   const CFreal gamma = _varSet->getModel()->getGamma();
   const CFreal gammaMinus1 = gamma - 1.0;
   const CFreal gammaDivGammaMinus1 = gamma/(gamma -1.0);
   const CFreal R = _varSet->getModel()->getR();

   const CFuint faceID = face->getID();
   const CFuint startID = faceID*PhysicalModelStack::getActive()->getDim();

   DataHandle<CFreal> normals = socket_normals.getDataHandle();

   CFreal nx = normals[startID];
   CFreal ny = normals[startID + 1];
   const CFreal invFaceLength = 1./sqrt(nx*nx + ny*ny);
   nx *= invFaceLength;
   ny *= invFaceLength;

   // set the physical data starting from the inner state
   _varSet->computePhysicalData(*innerState, _dataInnerState);

  computeBoundarySpeed(face);

   const CFreal u = _dataInnerState[EulerTerm::VX];
   const CFreal v = _dataInnerState[EulerTerm::VY];
   // unused // const CFreal rho = _dataInnerState[EulerTerm::RHO];
   const CFreal vn = u*nx + v*ny;
   // unused // const CFreal pInnerState = _dataInnerState[EulerTerm::P];
   const CFreal aInnerState = _dataInnerState[EulerTerm::A];
   // unused // const CFreal machInner = vn / aInnerState;

   const CFreal vnInf = _uInf*nx + _vInf*ny;
   const CFreal rhoInf = _pressure/(R*_temperature);
   const CFreal aInf = sqrt(gamma*_pressure/rhoInf);
   //Compute the Riemann invariants
   const CFreal RPlus_n = vn + (2.*aInnerState)/(gammaMinus1);
   const CFreal RMin_n = vnInf  - (2.*aInf)/(gammaMinus1);
   const CFreal normalSpeed = _speed[XX]*nx + _speed[YY]*ny;

   CFreal RMinRmax = 0.5 * (RPlus_n+ RMin_n) - normalSpeed;

   // depending on the sign and magnitude of the local Mach number,
   // number of variables to be specified are determined

   // supersonic outlet case
//    if (machInner >= 1.0) {
//      (*ghostState)[0] = (*innerState)[0];
//      (*ghostState)[1] = (*innerState)[1];
//      (*ghostState)[2] = (*innerState)[2];
//      (*ghostState)[3] = (*innerState)[3];
//    }
//
//    // supersonic inlet case
//    if (machInner <= -1.0) {
//      const CFreal rhoInf = _pressure/(R*_temperature);
//
//      // set all the physical data corresponding to the ghost state
//      _dataGhostState[EulerTerm::RHO] = 2.0*rhoInf - rho;
//      _dataGhostState[EulerTerm::VX] = 2.0*_uInf - u - _speed[XX];
//      _dataGhostState[EulerTerm::VY] = 2.0*_vInf - v - _speed[YY];
//      _dataGhostState[EulerTerm::P] = 2.0*_pressure - pInnerState;
//      _dataGhostState[EulerTerm::A] = sqrt(gamma*_dataGhostState[EulerTerm::P]/
// 					  _dataGhostState[EulerTerm::RHO]);
//      _dataGhostState[EulerTerm::V] = sqrt(_dataGhostState[EulerTerm::VX]*
// 					  _dataGhostState[EulerTerm::VX] +
// 					  _dataGhostState[EulerTerm::VY]*
// 					  _dataGhostState[EulerTerm::VY]);
//
//      _dataGhostState[EulerTerm::H] = (gammaDivGammaMinus1*_dataGhostState[EulerTerm::P]
// 				      + 0.5*_dataGhostState[EulerTerm::RHO]*
// 				      _dataGhostState[EulerTerm::V]*
// 				      _dataGhostState[EulerTerm::V])/
//        _dataGhostState[EulerTerm::RHO];
//
//      // set the ghost state starting from the physical data
//      _varSet->computeStateFromPhysicalData(_dataGhostState, *ghostState);
//    }

   // subsonic outlet case
   if (RMinRmax > 0.)  {

    CFreal vnInner = _dataInnerState[EulerTerm::VX]*nx + _dataInnerState[EulerTerm::VY] * ny;
    CFreal Ubound = _dataInnerState[EulerTerm::VX] + (((RPlus_n + RMin_n)*0.5) - vnInner)*nx;
    CFreal Vbound = _dataInnerState[EulerTerm::VY] + (((RPlus_n + RMin_n)*0.5) - vnInner)*ny;

     _dataGhostState[EulerTerm::RHO] = _dataInnerState[EulerTerm::RHO];
     _dataGhostState[EulerTerm::VX] = 2.0*Ubound - u;
     _dataGhostState[EulerTerm::VY] = 2.0*Vbound - v;
     _dataGhostState[EulerTerm::V] = sqrt(_dataGhostState[EulerTerm::VX]*
					  _dataGhostState[EulerTerm::VX] +
					  _dataGhostState[EulerTerm::VY]*
					  _dataGhostState[EulerTerm::VY]);
     _dataGhostState[EulerTerm::P] = _dataInnerState[EulerTerm::P];
     _dataGhostState[EulerTerm::H] = (gammaDivGammaMinus1*_dataGhostState[EulerTerm::P]
				      + 0.5*_dataGhostState[EulerTerm::RHO]*
				      _dataGhostState[EulerTerm::V]*
				      _dataGhostState[EulerTerm::V])/
       _dataGhostState[EulerTerm::RHO];
     _dataGhostState[EulerTerm::A] = sqrt(gamma*_dataGhostState[EulerTerm::P]/
					  _dataGhostState[EulerTerm::RHO]);
     
     _dataGhostState[EulerTerm::T] = _dataGhostState[EulerTerm::P]/(_dataGhostState[EulerTerm::RHO]*R);
     _varSet->computeStateFromPhysicalData(_dataGhostState, *ghostState);
   }

   // subsonic inlet case
   if (RMinRmax < 0.) {

    CFreal  Ubound = _uInf + (((RPlus_n + RMin_n)*0.5) - vnInf)*nx;
    CFreal  Vbound = _vInf + (((RPlus_n + RMin_n)*0.5) - vnInf)*ny;

     _dataGhostState[EulerTerm::RHO] = _pressure/(R*_temperature);
     _dataGhostState[EulerTerm::VX] = 2.0*Ubound - u;
     _dataGhostState[EulerTerm::VY] = 2.0*Vbound - v;
     _dataGhostState[EulerTerm::V] = sqrt(_dataGhostState[EulerTerm::VX]*
					  _dataGhostState[EulerTerm::VX] +
					  _dataGhostState[EulerTerm::VY]*
					  _dataGhostState[EulerTerm::VY]);
     _dataGhostState[EulerTerm::P] = _pressure;
     _dataGhostState[EulerTerm::H] = (gammaDivGammaMinus1*_dataGhostState[EulerTerm::P]
				      + 0.5*_dataGhostState[EulerTerm::RHO]*
				      _dataGhostState[EulerTerm::V]*
				      _dataGhostState[EulerTerm::V])/
       _dataGhostState[EulerTerm::RHO];

     _dataGhostState[EulerTerm::A] = sqrt(gamma*_dataGhostState[EulerTerm::P]/
					  _dataGhostState[EulerTerm::RHO]);
     
     _dataGhostState[EulerTerm::T] = _dataGhostState[EulerTerm::P]/(_dataGhostState[EulerTerm::RHO]*R);
     
     _varSet->computeStateFromPhysicalData(_dataGhostState, *ghostState);
   }
 }

//////////////////////////////////////////////////////////////////////////////

void UnsteadyFarFieldEuler2D::computeBoundarySpeed(GeometricEntity *const face)
{

  ///Compute the projection of the cell center on the boundary face
  // The equation of the plane containing the boundary face
  // and the given node (xp, yp, zp) (first node of the face),
  // with normal (a,b,c) is
  // a*x + b*y + c*z + k = 0
  // with k = -a*xp - b*yp - c*zp
  const CFuint dim = PhysicalModelStack::getActive()->getDim();
//unused//  const CFuint nbNodes = face->nbNodes();
  const Node* const node = face->getNode(0);
  const State* const state = face->getState(1);

  const CFuint faceID = face->getID();
  const CFuint startID = faceID*dim;
  RealVector faceNormal(dim);

  DataHandle< CFreal> normals = socket_normals.getDataHandle();

  // set the current normal
  for (CFuint iDim = 0; iDim < dim; ++iDim) {
    faceNormal[iDim] = normals[startID + iDim];
  }

  CFLogDebugMax( "faceNormal = " << faceNormal << "\n");
  CFLogDebugMax( "state coord = " << state->getCoordinates() << "\n");

  const CFreal k = - MathFunctions::innerProd(faceNormal, *node);

  // t is parameter for vectorial representation of a straight line
  // in space
  // t = (a*xM + b*yM + c*zM + k)/(a*a + b*b + c*c)

  const CFreal n2 = MathFunctions::innerProd(faceNormal, faceNormal);

  cf_assert(std::abs(n2) > 0.0);

  const RealVector& stateCoord = state->getCoordinates();
  const CFreal t = (MathFunctions::innerProd(faceNormal,stateCoord) + k)/n2;

  // The point N, projection from M (position of the other state,
  // internal neighbor of the face) with respect to the
  // given plane is given by
  // (xN, yN, zN) = (xM, yM, zM) - t*(a,b,c)

  for (CFuint iDim = 0; iDim < dim; ++iDim) {
    _coord[iDim] =  stateCoord[iDim] - t*faceNormal[iDim];
  }

  DataHandle<Node*> pastNodes = socket_pastNodes.getDataHandle();
  DataHandle<Node*> futureNodes = socket_futureNodes.getDataHandle();

  ///Compute the speed of the projected point from the boundary nodes
  // The point N now lies on the face.
  // We have to interpolate the speed at N from the speed of the nodes.
  // Do an averaging weighted by the distance...

  // for 2D just compute more easily
  const CFuint nodeID0 = face->getNode(0)->getLocalID();
  const CFuint nodeID1 = face->getNode(1)->getLocalID();
  const CFreal dt = SubSystemStatusStack::getActive()->getDT();

  const RealVector speedNode0 = (*(futureNodes[nodeID0]) - *(pastNodes[nodeID0]))/dt;
  const RealVector speedNode1 = (*(futureNodes[nodeID1]) - *(pastNodes[nodeID1]))/dt;

  const RealVector projectionToNode0 = ((*(face->getNode(0))) - _coord);
  const RealVector node1ToNode0 = ((*(face->getNode(1))) - (*(face->getNode(0))));
  const CFreal ratio = projectionToNode0.norm2()/node1ToNode0.norm2();
// std::cout << "Coord: " << _coord << std::endl;
// std::cout << "Node0: " << (*(face->getNode(0))) << std::endl;
// std::cout << "Node1: " << (*(face->getNode(1))) << std::endl;

  _speed = (ratio * (speedNode1 - speedNode0));
  _speed += speedNode0;

/*  RealVector sumCoordOverDistance(dim);
  RealVector sumOverDistance(dim);

  sumCoordOverDistance = 0.;
  sumOverDistance = 0.;

  for(CFuint iNode=0;iNode < nbNodes;iNode++)
  {
    for(CFuint iDim=0;iDim < dim;iDim++)
    {
      sumCoordOverDistance[iDim] += (*(face->getNode(iNode)))[iDim] / ((*(face->getNode(iNode)))[iDim] - _coord[iDim]);
      sumOverDistance[iDim] += 1./((*(face->getNode(iNode)))[iDim] - _coord[iDim]);
    }
  }

  for(CFuint iDim=0;iDim < dim;iDim++)
  {
    _speed[iDim] = sumCoordOverDistance[iDim] / sumOverDistance[iDim];
  }*/
}

//////////////////////////////////////////////////////////////////////////////

vector<SafePtr<BaseDataSocketSink> >
UnsteadyFarFieldEuler2D::needsSockets()
{
  vector<SafePtr<BaseDataSocketSink> > result =
    FVMCC_BC::needsSockets();

  result.push_back(&socket_pastNodes);
  result.push_back(&socket_futureNodes);

  return result;
}


//////////////////////////////////////////////////////////////////////////////

    } // namespace FiniteVolume

  } // namespace Numerics

} // namespace COOLFluiD
