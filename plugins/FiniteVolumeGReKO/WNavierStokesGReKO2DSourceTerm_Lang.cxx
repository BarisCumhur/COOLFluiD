#include "WNavierStokesGReKO2DSourceTerm_Lang.hh"
#include "GReKO/NavierStokes2DGReKOPuvt.hh"
#include "Common/CFLog.hh"
#include "Framework/GeometricEntity.hh"
#include "Framework/MeshData.hh"
#include "FiniteVolumeGReKO/FiniteVolumeGReKO.hh"
#include "Framework/SubSystemStatus.hh"

#include "Framework/MethodStrategyProvider.hh"
#include "FiniteVolume/CellCenterFVMData.hh"
#include "FiniteVolume/DerivativeComputer.hh"
#include "MathTools/MathConsts.hh"
//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Physics::NavierStokes;
using namespace COOLFluiD::Physics::GReKO;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace FiniteVolume {

//////////////////////////////////////////////////////////////////////////////

MethodStrategyProvider<WNavierStokesGReKO2DSourceTerm_Lang,
		       CellCenterFVMData,
		       ComputeSourceTerm<CellCenterFVMData>,
		       FiniteVolumeGReKOModule>
WNavierStokesGReKO2DSourceTerm_LangFVMCCProvider("WNavierStokesGReKO2DSourceTerm_Lang");

//////////////////////////////////////////////////////////////////////////////
void WNavierStokesGReKO2DSourceTerm_Lang::defineConfigOptions(Config::OptionList& options)
{
 options.addConfigOption< bool >("SSTV","True for SST with Vorticity source term");
 options.addConfigOption< bool >("SSTsust","True for SST with  sustaining terms");
 options.addConfigOption< CFreal >("Kinf","K at the farfield");
 options.addConfigOption< CFreal >("Omegainf","Omega at the farfield");
}

//////////////////////////////////////////////////////////////////////////////

WNavierStokesGReKO2DSourceTerm_Lang::WNavierStokesGReKO2DSourceTerm_Lang(const std::string& name) :
  ComputeSourceTermFVMCC(name),
  _varSet(CFNULL),
  _diffVarSet(CFNULL),
  _temp(),
  _avState(),
  _physicalData(),
  _nstates(CFNULL),
  _wallDistance(CFNULL),
  _values(),
  _states(),
  _unperturbedPositivePart(),
  _unperturbedNegativePart(),
  _gradients(),
 _Theta(2),
  _Meanalue(2),
  _Flambdak(),
  _Flambdakprime(),
  _Rethetat(),
  _Rethetac(),
  _Flength(),
  _PGrad(1)

{ 
 addConfigOptionsTo(this);
  _SST_V = false;
  setParameter("SSTV",&_SST_V);
  _SST_sust = false;
  setParameter("SSTsust",&_SST_sust);
  _kamb = 100. ;
  setParameter("Kinf",&_kamb);
  _omegaamb = 0.1;
  setParameter("Omegainf",&_omegaamb);
}

//////////////////////////////////////////////////////////////////////////////

WNavierStokesGReKO2DSourceTerm_Lang::~WNavierStokesGReKO2DSourceTerm_Lang()
{
  for(CFuint iGrad = 0; iGrad < _gradients.size(); iGrad++)
  {
    deletePtr(_gradients[iGrad]);
  }

}

//////////////////////////////////////////////////////////////////////////////

void WNavierStokesGReKO2DSourceTerm_Lang::setup()
{
  CFAUTOTRACE;

  ComputeSourceTermFVMCC::setup();
  
  _varSet = getMethodData().getUpdateVar().d_castTo<Euler2DGReKOVarSet>();
  _diffVarSet = getMethodData().getDiffusiveVar().d_castTo<NavierStokes2DGReKOPuvt>();
  
  _temp.resize(PhysicalModelStack::getActive()->getNbEq());
  _avState.resize(PhysicalModelStack::getActive()->getNbEq());

  cf_assert(_varSet.isNotNull());
  _varSet->getModel()->resizePhysicalData(_physicalData);

  _nstates = _sockets.getSocketSink<RealVector>("nstates")->getDataHandle();
  _wallDistance = _sockets.getSocketSink<CFreal>("wallDistance")->getDataHandle();

 SafePtr<DerivativeComputer> derComput =
    this->getMethodData().getDerivativeComputer();
 const CFuint nbNodesInControlVolume =
    derComput->getMaxNbVerticesInControlVolume();

  _values.resize(PhysicalModelStack::getActive()->getNbEq(), nbNodesInControlVolume);
  _states.reserve(PhysicalModelStack::getActive()->getNbEq());
//  const CFuint maxNbNodesIn2DCV = 4;
//  _states.reserve(maxNbNodesIn2DCV);

  const CFuint nbScalarVars = _varSet->getModel()->getNbScalarVars(0);
  _unperturbedPositivePart.resize(nbScalarVars);
  _unperturbedNegativePart.resize(nbScalarVars);

  _gradients.resize(PhysicalModelStack::getActive()->getNbEq());
  for(CFuint iGrad = 0; iGrad < _gradients.size(); iGrad++)
  {
    _gradients[iGrad] = new RealVector(DIM_2D);
  }
}

//////////////////////////////////////////////////////////////////////////////

void WNavierStokesGReKO2DSourceTerm_Lang::computeSource(Framework::GeometricEntity *const element,
						   RealVector& source,
						   RealMatrix& jacobian)
{
  DataHandle<CFreal> normals  = this->socket_normals.getDataHandle();
  //cout << "normals" << endl;
  //DataHandle<CFreal> normals = 
  //_sockets.getSocketSink<CFreal>("normals")->getDataHandle();
  DataHandle<CFint> isOutward = this->socket_isOutward.getDataHandle();
  DataHandle<CFreal> volumes = socket_volumes.getDataHandle();
 
 
  cf_assert(_varSet.isNotNull());

  // Set the physical data for the cell considered
  State *const currState = element->getState(0);
  _varSet->computePhysicalData(*currState, _physicalData);

  // fill in the nodal states
  const vector<Node*>* const nodes = element->getNodes();
  const CFuint nbNodesInElem = nodes->size();
  _states.clear();
  for (CFuint i = 0; i < nbNodesInElem; ++i) {
    _states.push_back(&_nstates[(*nodes)[i]->getLocalID()]);
  }

  //From now on, we will use the gradient vars
  _diffVarSet->setGradientVars(_states, _values, _states.size());

  const vector<GeometricEntity*>& faces = *element->getNeighborGeos();
  cf_assert(faces.size() == nbNodesInElem);
  const CFuint elemID = element->getID();

  // compute the gradients by applying Green Gauss in the
  // cell d's
  for(CFuint iGrad = 0; iGrad < _gradients.size(); iGrad++)
  {
    *(_gradients[iGrad]) = 0.0;
  }

  const CFuint nbEqs = PhysicalModelStack::getActive()->getNbEq();

  for (CFuint i = 0; i < nbNodesInElem; ++i) {
    // get the face normal
    const CFuint faceID = faces[i]->getID();
    const CFuint startID = faceID*PhysicalModelStack::getActive()->getDim();
    CFreal nx = normals[startID];
    CFreal ny = normals[startID + 1];
    if (static_cast<CFuint>( isOutward[faceID]) != elemID) {
      nx *= -1.;
      ny *= -1.;
    }

    if (i < (nbNodesInElem - 1))
    {
      for (CFuint iEq = 0; iEq < nbEqs; ++iEq) {
	(*(_gradients[iEq]))[XX] += nx*(_values(iEq,i) + _values(iEq,i+1));
	(*(_gradients[iEq]))[YY] += ny*(_values(iEq,i) + _values(iEq,i+1));
      }
    }
    else {
      for (CFuint iEq = 0; iEq < nbEqs; ++iEq) {
	(*(_gradients[iEq]))[XX] += nx*(_values(iEq,i) + _values(iEq,0));
	(*(_gradients[iEq]))[YY] += ny*(_values(iEq,i) + _values(iEq,0));
      }
    }
  }


  // if (SubSystemStatusStack::getActive()->getNbIter() == 2) {
//
// if(element->getID() == (2301+15452) )
// // if(element->getID() == (1503+15452) )
// {
//   CFout << "\n----------------\nLower --- Cell  #" << element->getID() <<"\n";
//   CFout << "gradK [x,y]  : " << (*(_gradients[4])) <<"\n";
// //   CFout << "With the nodes: " << _p <<"\n";
//   State *const currState = element->getState(0);
// //   State *const innerState = face->getState(0);
//   CFout << "centroid [x,y]  : " << currState->getCoordinates() <<"\n";
// // innerState->getCoordinates()
// //   const Node* const node = nodes[trs->getNodeID(iGeo, 0)];
//
//   for (CFuint i = 0; i < nbNodesInElem; ++i) {
//     const CFuint faceID = faces[i]->getID();
//     const CFuint startID = faceID*PhysicalModelStack::getActive()->getDim();
//     CFout << "normal X Y: " << normals[startID] << "\n" << "            " << normals[startID + 1] << "\n" ;
//   }
// }
//
// if(element->getID() == (5698+15452) )
// // if(element->getID() == (6496+15452) )
// {
//   CFout << "\n----------------\nUpper --- Cell  #" << element->getID() <<"\n";
//   CFout << "gradK [x,y]  : " << (*(_gradients[4])) <<"\n";
//   State *const currState = element->getState(0);
//   CFout << "centroid [x,y]  : " << currState->getCoordinates() <<"\n";
//
//   for (CFuint i = 0; i < nbNodesInElem; ++i) {
//     const CFuint faceID = faces[i]->getID();
//     const CFuint startID = faceID*PhysicalModelStack::getActive()->getDim();
//     CFout << "normal X Y: " << normals[startID] << "\n" << "            " << normals[startID + 1] << "\n" ;
//   }
// }
// }



  *(_gradients[0]) *= 0.5/volumes[elemID];
  *(_gradients[1]) *= 0.5/volumes[elemID];
  *(_gradients[2]) *= 0.5/volumes[elemID];
  *(_gradients[3]) *= 0.5/volumes[elemID];
  *(_gradients[4]) *= 0.5/volumes[elemID];
  *(_gradients[5]) *= 0.5/volumes[elemID];
  *(_gradients[6]) *= 0.5/volumes[elemID];
  *(_gradients[7]) *= 0.5/volumes[elemID];

// compute PUVTGReKO by averaging the nodes
// NO!!! If we do it like that we nearly certainly
// get negative values!!!
// So we just take the state value
  const CFuint iK = _varSet->getModel()->getFirstScalarVar(0);

  _avState[0] = _physicalData[EulerTerm::P];
  _avState[1] = _physicalData[EulerTerm::VX];
  _avState[2] = _physicalData[EulerTerm::VY];
  _avState[3] = _physicalData[EulerTerm::T];
  _avState[4] = _physicalData[iK];
  _avState[5] = _physicalData[iK+1];
  _avState[6] = _physicalData[iK+2];
  _avState[7] = _physicalData[iK+3];

  double avu     = _physicalData[EulerTerm::VX];
  double avv     = _physicalData[EulerTerm::VY];
  double avK     = _physicalData[iK];
  double avOmega = _physicalData[iK+1];
  double avGa    = _physicalData[iK+2];
  double avRe    = _physicalData[iK+3];

  CFreal avV     = _physicalData[EulerTerm::V];
  CFreal VSound  = _physicalData[EulerTerm::A];
  CFreal avH     = _physicalData[EulerTerm::H];
  
  const CFreal rho = _diffVarSet->getDensity(_avState);

  ///Get the wall distance
  const CFreal avDist = _wallDistance[currState->getLocalID()];

  ///Set the wall distance before computing the turbulent viscosity
  _diffVarSet->setWallDistance(avDist);
  CFreal mut = _diffVarSet->getTurbDynViscosityFromGradientVars(_avState, _gradients);
   CFreal mu = _diffVarSet->getLaminarDynViscosityFromGradientVars(_avState);
  _diffVarSet->computeBlendingCoefFromGradientVars(_avState, *(_gradients[4]), *(_gradients[5]));

  //The MOdified Blending Function F1 
  const CFreal F1org  = _diffVarSet->getBlendingCoefficientF1();
  const CFreal Ry     = (rho*avDist * sqrt(avK) )/(mu);   
  const CFreal F3bis  = std::pow(Ry/120.0,8);
  const CFreal F3     = exp(-F3bis);
  const CFreal blendingCoefF1     = std::max(F1org,F3);
  
  //const CFreal blendingCoefF1 = _diffVarSet->getBlendingCoefficientF1();
 
  const CFreal sigmaOmega2 = _diffVarSet->getSigmaOmega2();


  ///Compute Reynolds stress tensor
  const CFreal twoThirdRhoK = (2./3.)*(avK * rho);
  const CFreal twoThirdDivV = (2./3.)*((*(_gradients[1]))[XX] + (*(_gradients[2]))[YY]);

  const CFreal coeffTauMu = _diffVarSet->getModel().getCoeffTau();
  CFreal tauXX = coeffTauMu * (mut * (2.*(*(_gradients[1]))[XX] - twoThirdDivV)) - twoThirdRhoK;
  CFreal tauXY = coeffTauMu * (mut * ((*(_gradients[1]))[YY] + (*(_gradients[2]))[XX]));
  CFreal tauYY = coeffTauMu * (mut * (2.*(*(_gradients[2]))[YY] - twoThirdDivV)) - twoThirdRhoK;

// tauXX = max(0., tauXX);
// tauXY = max(0., tauXY);
// tauYY = max(0., tauYY);

////cout << "AVDIS" << avDist<< endl;
  
  const CFreal vorticity1 = 0.5*((*(_gradients[2]))[XX] - (*(_gradients[1]))[YY]); 
  const CFreal vorticity = std::sqrt(2*vorticity1*vorticity1);

//const CFreal vorticity = fabs((*(_gradients[2]))[XX] - (*(_gradients[1]))[YY]);

   ///Compute the blending function Fthetat
  const CFreal  Rew         = (rho*avDist * avDist * avOmega)/(mu);   
  const CFreal  Fwake1      = (1e-5 * Rew)*(1e-5 * Rew);   
  const CFreal  Fwake       = exp(-Fwake1);
  const CFreal  thetaBL     = (avRe*mu)/(rho*avV);
  const CFreal  deltaBL     = (0.5*15*thetaBL);
  const CFreal  delta       = (50 * vorticity * avDist * deltaBL)/(avV);
  const CFreal  CoefFtheta0 = (avDist/delta)*(avDist/delta)*(avDist/delta)*(avDist/delta);
  const CFreal  CoefFtheta1 = exp(-CoefFtheta0);
  const CFreal  Ftheta1     = Fwake * CoefFtheta1;
  const CFreal  ce2         = 50;
  //const CFreal  overce2     = 1/50;
  //const CFreal  Ftheta2     = (avGa-overce2)/(1.0-overce2);
  const CFreal  Ftheta3     = 1-(((ce2*avGa-1.0)/(ce2-1.0))*((ce2*avGa-1.0)/(ce2-1.0)));
  const CFreal  Ftheta4     = std::max(Ftheta1,Ftheta3);
  const CFreal   Fthetat     = std::min(Ftheta4,1.0);


   
  //The variables needed for the  production term of Re
  const CFreal cthetat   = 0.03;
  const CFreal t         = (500 * mu )/(rho * avV * avV);
  //const CFreal Tu       = 100 * (std::sqrt(2*avK/3))/(avV);
   cf_assert(avV >0.);   
      CFreal Tu= 100 * (std::sqrt(2*avK/3))/(avV);
      //CFreal Tu= (avV >= 1e-8) ? 100 * (std::sqrt(2*avK/3))/(avV) : 0.;
    //   CFreal Tu       = 100 * (std::sqrt(2*avK/3))/(std::max(avV,1e-12));
  //const CFreal Tu        = std::min(Tu1,100.0);
  //const CFreal Tu        = std::max(Tu1,0.027);
  //const CFreal Tu        = std::max(Tu2,0.027497);
  //const CFreal Tu        = std::max(Tu2,.00081773);
 // const CFreal Tu       = 100 * (std::sqrt(2*0.0001/3))/932;
  //const CFreal Tu        = 100*(std::sqrt(2*33./3))/696.;
   cf_assert(Tu > 0);   
  const CFreal overTu    =  1/Tu;
  //const CFreal overTu    = (avV>=1e-8) ? 1/Tu : 0.;

    if(_PGrad == 1){
           if (Tu<=1.3) {
                _Rethetat = (1173.51-589.428*Tu + 0.2196*overTu*overTu);
		 }
    	  else {
  		const CFreal lamco5   = Tu - 0.5658;
  	 	const CFreal pwtu   = -0.671;
  	 	_Rethetat = 331.5*std::pow(lamco5,pwtu);
         	 }

      } 

   else {
	//----------------------------- --------------------------------------------------------------------------------------------
	//
	//                                               |  PRESS  GRAD   |
	//


 	cout << "thetaN" << _Theta[0] <<endl; 
  	if(Tu <=1.3){
    	_Theta[0]            = (mu/(rho*avV))*(1173.51-589.428*Tu + 0.2196*overTu*overTu);
 	//cout << "thetaN" << _Theta[0] <<endl; 
  	}
  	else {
   	const CFreal lamco5   = Tu - 0.5658;
   	const CFreal pwtu     = -0.671;
    	_Theta[0]            = (mu/(rho*avV))*331.5*std::pow(lamco5,pwtu);
 	//cout << "thetaP" <<  _Theta[0] << endl; 
   	}
  	const CFuint MAXITER   = 10;
  	const CFreal TOL       = 1e-6;
  	CFreal Restheta        =  _Theta[0]*TOL;
 	for (CFuint i = 0; i < MAXITER; ++i)
 	{
	  //cout << "ITER" << i  << endl;
 	 //The variables needed for the calculayion of Re_thetat
  	const CFreal dUdx   	 = avV * (avu* (*(_gradients[1]))[XX]  + avv * (*(_gradients[2]))[XX]);
  	const CFreal dUdy      = avV * (avu* (*(_gradients[1]))[YY]  + avv * (*(_gradients[2]))[YY]);
  	const CFreal overU     = 1./avV;
  	const CFreal overmu    = 1./mu;
  	const CFreal dUds      =  overU * (avu * dUdx + avv *dUdy);
  	const CFreal MACH      = avV/VSound;
  	const CFreal Fc1       = 1+16*(2*MACH)*(2*MACH)*(2*MACH)*(2*MACH);
  	const CFreal Fc        = std::pow(Fc1,0.75);
   	CFreal lambda1  = rho * _Theta[0]* _Theta[0] * overmu * dUds*Fc;
   	CFreal lambda2 = std::max(lambda1,-0.1);
   	CFreal lambda =  std::min(lambda2,0.1);
       
 	 const CFreal lambdaprime = 2 * rho * _Theta[0] * overmu * dUds;


  	 if (lambda <=0) {
   	const CFreal lamco1   = (Tu/1.5);
   	const CFreal lamco2   = -1.0*std::pow(lamco1,1.5);
   	const CFreal FlambN   =  -12.986 * lambda - 123.66 * lambda*lambda -405.689 * lambda*lambda*lambda;
   	const CFreal FlambprimeN   = -12.986 - 2 * 123.66 * lambda*lambdaprime - 3* 405.689 * lambda*lambda*lambdaprime;
   	 _Flambdak      = 1 - FlambN * std::exp(lamco2);
    	_Flambdakprime =  -1.0*FlambprimeN * std::exp(lamco2);
 	 }
   	else {
   	const CFreal lamco3   = -1.0*(Tu/0.5);
   	const CFreal lamco4   = -35.0*lambda;
   	const CFreal FlambP   = 0.275*(1-std::exp(lamco4))*std::exp(lamco3);
   	const CFreal FlambprimeP   = 0.275*(1-(-35.0*lambdaprime)*std::exp(lamco4))*std::exp(lamco3); 
    	_Flambdak= 1 + FlambP;
    	_Flambdakprime= FlambprimeP;
   	}
 
  	if (Tu<=1.3) {
    	 CFreal Rethetat0 = (1173.51-589.428*Tu + 0.2196*overTu*overTu);
   	// const CFreal Rethetatprime = Rethetat0 * _Flambdakprime;
         _Rethetat = Rethetat0 * _Flambdak;
  	}
   	else {
   	const CFreal lamco5   = Tu - 0.5658;
   	const CFreal pwtu   = -0.671;
   	const CFreal Rethetat0 = 331.5*std::pow(lamco5,pwtu);
  	// const CFreal Rethetatprime = Rethetat0 * _Flambdakprime;
         _Rethetat = Rethetat0 * _Flambdak;
 	 }

   	const CFreal Rethetatprime =  (_Rethetat* _Flambdakprime)/_Flambdak;

     
   	const CFreal MainF = _Rethetat -(rho*avV)*_Theta[0]/mu;
   	const CFreal MainFprime = Rethetatprime -(rho*avV)/mu;
    
    	_Theta[1] = _Theta[0] - MainF/MainFprime;
  	if ( std::abs(_Theta[0]-_Theta[1]) <= Restheta ) break;
    	_Theta[0]= _Theta[1];
   	//cout << "diff   " << _Theta[0]-_Theta[1] << endl;
   	// if ( i == MAXITER -1 ) cout << "GREK0: Newton convergence was  reached! at    " << i<< endl;
    	//if ( i == MAXITER - 1) cout << "GREK0: Newton convergence was  reached!" << endl;
  	}
 
 
  	//Compute Flength
 	//const CFreal FLENGTH1  = 0.1*exp(-0.022*avRe+12)+0.45;
      	// _Flength  = 8.5e7*std::pow(_Rethetat,-3)*(1+0.25*MACH*MACH*MACH*MACH);
//	}
	//---------------------------------------------------------------------------------------------------------------------
	//             
	//
	//
	//-----------------------------------------------------------------------------------------------------------------------
      }
  const CFreal MACH = avV/VSound;

       if (avRe <=400){  
            _Flength = 398.189*1e-1 -119.270*1e-4*avRe -132.567*1e-6*avRe*avRe;   
          }
      else if ((avRe>=400 ) && (avRe < 596)) {
            _Flength = 263.404 - 123.939*1e-2*avRe + 194.548*1.e-5*avRe*avRe - 101.695*1e-8*avRe*avRe*avRe;
              }
        else if ((avRe>=596 ) && (avRe < 1200)) {
            _Flength = 0.5-(avRe - 596.0)*3.0*1e-4;
              }
        else {
            _Flength = 0.3188;
          }
 
 
 //Compute Rethtetac
        //_Rethetac  = _Rethetat*(-4.45e4*_Rethetat+0.92);
        //_Rethetac  = _Rethetat*(-4.45e4*_Rethetat+0.92);
        //_Rethetac  = _Rethetat*(-4.45e4*_Rethetat+0.92);

        if (avRe <= 1860){
            _Rethetac  = avRe - (396.035*1e-2 -120.656*1e-4*avRe)+(868.230*1e-6)*avRe*avRe - 696.506*1e-9*avRe*avRe*avRe + 174.105*1e-12*avRe*avRe*avRe*avRe;
           }
        else {
            _Rethetac = avRe - 593.11 + (avRe - 1870.0)*0.482;
              }
           
      
  //ompute Gasep
   const CFreal Rt         = (rho*avK)/(mu*avOmega);
   const CFreal Freattach0 = exp(-Rt/20);
   const CFreal Freattach  = std::pow(Freattach0,4);;
   const CFreal strain1     = 0.5*((*(_gradients[2]))[XX] + (*(_gradients[1]))[YY]);
   const CFreal strain     = std::sqrt(2*strain1*strain1);
   const CFreal Rev        = (rho*avDist*avDist*strain)/(mu);
   const CFreal Gasep1     = ((Rev)/((3.235 *  _Rethetac)))-1;
   const CFreal Gasep2     = std::max(0.,Gasep1);
   const CFreal Gasep3     = 2.0*Gasep2*Freattach;
   const CFreal Gasep4     = std::min(Gasep3,2.0);
   const CFreal Gasep      = Gasep4*Fthetat;
   

  ///Gaeff
   const CFreal Gaeff  = std::max(avGa,Gasep);
   

 
  // The Onset function of the  production term of the intermittency Ga

  const CFreal  Fonset1 = (Rev )/(2.93*_Rethetac);
  const CFreal  Fonset2 = std::pow(Fonset1,4);
  const CFreal  Fonset3 = std::max(Fonset1,Fonset2);
  const CFreal  Fonset4 = std::min(Fonset3,2.0);
  const CFreal  Fonset6 = 1-((Rt/2.5)*(Rt/2.5)*(Rt/2.5)) ;
  const CFreal  Fonset7 = std::max(Fonset6,0.);
  const CFreal  Fonset8 = (Fonset4 -Fonset7);
  const CFreal  Fonset  = std::max(Fonset8,0.);

   
  ///The Modified Production term: k
  
  
  CFreal prodTerm_k = Gaeff*(tauXX*(*(_gradients[1]))[XX] + tauYY*(*(_gradients[2]))[YY] + tauXY * ((*(_gradients[1]))[YY] + (*(_gradients[2]))[XX]));
  //CFreal prodTerm_k = (tauXX*(*(_gradients[1]))[XX] + tauYY*(*(_gradients[2]))[YY] + tauXY * ((*(_gradients[1]))[YY] + (*(_gradients[2]))[XX]));

  ///Production term: Omega
  CFreal prodTerm_Omega = (_diffVarSet->getGammaCoef()*rho/mut)*
                          (tauXX*(*(_gradients[1]))[XX] + tauYY*(*(_gradients[2]))[YY] + tauXY * ((*(_gradients[1]))[YY] + (*(_gradients[2]))[XX]));

  ///This is used in (BSL,SST), not for normal kOmega
  const CFreal overOmega = 1./avOmega;
  prodTerm_Omega += (1. - blendingCoefF1) * 2. * rho * overOmega * sigmaOmega2
                    * ((*(_gradients[4]))[XX]*(*(_gradients[5]))[XX] + (*(_gradients[4]))[YY]*(*(_gradients[5]))[YY]);

  ///The Modified Destruction term: k
  ///CoeffDk This coefficient is used in the destruciton term related to k: 
    const CFreal CoeffDk1  = std::max(Gaeff,0.1);
    const CFreal CoeffDk   = std::min(CoeffDk1,1.0);
    CFreal destructionTerm_k = CoeffDk*((-1.) * rho * avOmega * avK * _diffVarSet->getBetaStar(_avState));
  // CFreal destructionTerm_k = ((-1.) * rho * avOmega * avK * _diffVarSet->getBetaStar(_avState));

  ///Destruction term: Omega
  CFreal destructionTerm_Omega = (-1.) * rho * avOmega * avOmega * _diffVarSet->getBeta(_avState);

  //Make sure negative values dont propagate...
  destructionTerm_k     = min(0., destructionTerm_k);
  destructionTerm_Omega = min(0., destructionTerm_Omega);
  prodTerm_k            = max(0., prodTerm_k);
  prodTerm_Omega        = max(0., prodTerm_Omega);



  // This trick was used by W. Dieudonne in Euphoria (with 20.)
  prodTerm_k = min(10.*fabs(destructionTerm_k), prodTerm_k);
  prodTerm_Omega = min(10.*fabs(destructionTerm_Omega), prodTerm_Omega);

   // The production term of the intermittency Ga
  const CFreal ca1       = 2.0;
  const CFreal ce1       = 1.0;
  const CFreal  ca2 =  0.06;
  const CFreal GaFonset1 = avGa * Fonset;
  const CFreal GaFonset  = std::pow(GaFonset1,0.5);

  const CFreal  Fturb1 =  exp(-Rt/4); 
  const CFreal  Fturb =  std::pow(Fturb1,4); 
    CFreal prodTerm_Ga = _Flength * ca1 * rho * strain * GaFonset * (1.0 - ce1*avGa);
      
  // CFreal prodTerm_Ga = _Flength * ca1 * rho * strain * GaFonset * 1.0 +  ca2 * rho *  vorticity * avGa * Fturb * (ce2*avGa);

  // The production term of  Re
    CFreal prodTerm_Re = cthetat * (rho/t) * (_Rethetat - avRe) * (1.0 - Fthetat);
   // CFreal prodTerm_Re = cthetat * (rho/t) * (_Rethetat) * (1.0 - Fthetat);

  //The variables needed for the  Destruction term of Ga   
  //Destruction term of the intermittency Ga
    //  CFreal  destructionTerm_Ga  = (-1.0) *ca2 * rho *  vorticity * avGa * Fturb - _Flength * ca1 * rho * strain * GaFonset * 1.0 * ce1*avGa;
      CFreal  destructionTerm_Ga  = (-1.0) *ca2 * rho *  vorticity * avGa * Fturb * (ce2*avGa - 1);
  

  //Destruction term of the intermittency Re
     CFreal destructionTerm_Re = 0;
   // CFreal  destructionTerm_Re  = cthetat * (rho/t) * (-1*avRe) * (1.0 - Fthetat);
   if(avGa>1.1){
    cout << "  avGa "<< avGa << "    " << avDist  << " prodTerm_Ga " << prodTerm_Ga << " prodTerm_Re " << prodTerm_Re << " destructionTerm_Ga " << destructionTerm_Ga << " destructionTerm_Re " << destructionTerm_Re << endl; 
   // cout << ""<< avGa<< " " << prodTerm_Ga << " "  << destructionTerm_Ga << endl; 
}

   cout << prodTerm_Ga << "    " << prodTerm_Re << endl;
  ///Make sure negative values dont propagate
   //prodTerm_Ga               = (prodTerm_Ga>1e-16) ? prodTerm_Ga :0. ;
  // prodTerm_Re               = (prodTerm_Re>1e-16) ?  prodTerm_Re :0. ;
  // destructionTerm_Ga        = (destructionTerm_Ga<-1e-16) ? destructionTerm_Ga :0. ;
  // destructionTerm_Re        = (destructionTerm_Re<-1e-16) ? destructionTerm_Re :0. ;
   prodTerm_Ga        = max(0., prodTerm_Ga);
   prodTerm_Re        = max(0., prodTerm_Re);
   destructionTerm_Ga = min(0., destructionTerm_Ga);
  // destructionTerm_Re = min(0., destructionTerm_Re);
   destructionTerm_Re = min(0., prodTerm_Re);

//    cout << "AFTER  avGa "<< avGa << "    " << avDist  << " prodTerm_Ga " << prodTerm_Ga << " prodTerm_Re " << prodTerm_Re << " destructionTerm_Ga " << destructionTerm_Ga << " destructionTerm_Re " << destructionTerm_Re << endl; 

  //prodTerm_Ga = min(10.*fabs(destructionTerm_Ga), prodTerm_Ga);
 // prodTerm_Re = min(10.*fabs(destructionTerm_Re), prodTerm_Re);

  //Computation of the source term
  source[0] = 0.0;
  source[1] = 0.0;
  source[2] = 0.0;
  source[3] = 0.0;

  //What we do with the source term depends if
  //we are computing the jacobian or not
  const bool isPerturb = this->getMethodData().isPerturb();
  const CFuint iPerturbVar = this->getMethodData().iPerturbVar();
  if(isPerturb)
  {
    /// Compute the jacobian contribution
    // only perturb the negative part of the source term
    if(iPerturbVar == 4)
    {
      source[4] = destructionTerm_k;
      source[4] += _unperturbedPositivePart[0];
    }
    else
    {
      source[4] = _unperturbedNegativePart[0];
      source[4] += _unperturbedPositivePart[0];
    }

    if(iPerturbVar == 5)
    {
      source[5] = destructionTerm_Omega;
      source[5] += _unperturbedPositivePart[1];
    }
    else
    {
      source[5] = _unperturbedNegativePart[1];
      source[5] += _unperturbedPositivePart[1];
    }


    if(iPerturbVar == 6)
    {
      source[6] = destructionTerm_Ga;
      source[6] += _unperturbedPositivePart[2];
    }
    else
    {
      source[6] = _unperturbedNegativePart[2];
      source[6] += _unperturbedPositivePart[2];
    }
    if(iPerturbVar == 7)
    {
      source[7] = destructionTerm_Re;
      source[7] += _unperturbedPositivePart[3];
    }
    else
    {
      source[7] = _unperturbedNegativePart[3];
      source[7] += _unperturbedPositivePart[3];
    }


  }
  else
  {
    /// Compute the rhs contribution
    // and Store the unperturbed source terms
    source[4] = prodTerm_k;
    source[4] += destructionTerm_k;
    _unperturbedPositivePart[0] = prodTerm_k;
    _unperturbedNegativePart[0] = destructionTerm_k;

    source[5] = prodTerm_Omega;
    source[5] += destructionTerm_Omega;
    _unperturbedPositivePart[1] = prodTerm_Omega;
    _unperturbedNegativePart[1] = destructionTerm_Omega;
    
    source[6] = prodTerm_Ga;
    source[6] += destructionTerm_Ga;
    _unperturbedPositivePart[2] = prodTerm_Ga;
    _unperturbedNegativePart[2] = destructionTerm_Ga;
    
    source[7] = prodTerm_Re;
    source[7] += destructionTerm_Re;
    _unperturbedPositivePart[3] = prodTerm_Re;
    _unperturbedNegativePart[3] = destructionTerm_Re;
  }
  ///Finally multiply by the cell volume
  source *= volumes[elemID];

  //if (SubSystemStatusStack::getActive()->getNbIter() < 20) source = 0.;

// if (SubSystemStatusStack::getActive()->getNbIter() == 20) {
// 	if(element->getID() == (2301+15452) )
// // 	if(element->getID() == (1503+15452) )
// 	{
// 		CFout << "\n----------------\nLower --- Cell  #" << element->getID() <<"\n";
// 		CFout << "gradK [x,y]  : " << (*(_gradients[4])) <<"\n";
// 		CFout << "K  : " << _K <<"\n";
// 		CFout << "p  : " << _p <<"\n";
// 		CFout << "u  : " << _u <<"\n";
// 		CFout << "v  : " << _v <<"\n";
// 		CFout << "T  : " << _T <<"\n";
// 		CFout << "total K source contribution  : " << source[4] <<"\n";
// 		CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// 		//   CFout << "Volume  : " << element->computeVolume() <<"\n";
// 		//   CFout << "StateID  : " << element->getState(0)->getLocalID() <<"\n";
// 	}
//
// 	if(element->getID() == (5698+15452) )
// // 	if(element->getID() == (6496+15452) // 	{
// 		  CFout << "\n----------------\nUpper --- Cell  #" << element->getID() <<"\n";
// 		//   CFout << "gradK [x,y]  : " << (*(_gradients[4])) <<"\n";
// 		CFout << "K  : " << _K <<"\n";
// 		CFout << "p  : " << _p <<"\n";
// 		CFout << "u  : " << _u <<"\n";
// 		CFout << "v  : " << _v <<"\n";
// 		CFout << "T  : " << _T <<"\n";
// 		CFout << "total K source contribution  : " << source[4] <<"\n";
// 		CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// 		//   CFout << "Volume  : " << element->computeVolume() <<"\n";
// 		//   CFout << "StateID  : " << element->getState(0)->getLocalID() <<"\n";
// 	}
//
// }


// const CFint Bunka = 5739;
// if(element->getID() == (Bunka+15452) )
// {
//   CFout << "---------------------\n------111111111------\n---------------------\n" ;
//   CFout << "\n-2300-----------\nLower --- Cell  #" << element->getID() <<"\n";
// //   CFout << "K  : " << _K <<"\n";
//   CFout << "total K source contribution  : " << source[4] <<"\n";
//   CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// }
// if(element->getID() == (Bunka+1+15452) )
// {
//   CFout << "\n-2301-----------\nLower --- Cell  #" << element->getID() <<"\n";
// //   CFout << "K  : " << _K <<"\n";
//   CFout << "total K source contribution  : " << source[4] <<"\n";
//   CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// }
// if(element->getID() == (Bunka+2+15452) )
// {
//   CFout << "\n-2302-----------\nLower --- Cell  #" << element->getID() <<"\n";
// //   CFout << "K  : " << _K <<"\n";
//   CFout << "total K source contribution  : " << source[4] <<"\n";
//   CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// }
//
// }
//
//
// if (SubSystemStatusStack::getActive()->getNbIter() == 2) {
// const CFint Bunka = 5739;
// if(element->getID() == (Bunka+15452) )
// {
//   CFout << "---------------------\n------222222222------\n---------------------\n" ;
//   CFout << "\n-2300-----------\nLower --- Cell  #" << element->getID() <<"\n";
// //   CFout << "K  : " << _K <<"\n";
//   CFout << "total K source contribution  : " << source[4] <<"\n";
//   CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// }
// if(element->getID() == (Bunka+1+15452) )
// {
//   CFout << "\n-2301-----------\nLower --- Cell  #" << element->getID() <<"\n";
// //   CFout << "K  : " << _K <<"\n";
//   CFout << "total K source contribution  : " << source[4] <<"\n";
//   CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// }
// if(element->getID() == (Bunka+2+15452) )
// {
//   CFout << "\n-2302-----------\nLower --- Cell  #" << element->getID() <<"\n";
// //   CFout << "K  : " << _K <<"\n";
//   CFout << "total K source contribution  : " << source[4] <<"\n";
//   CFout << "centroid [x,y]  : " << element->getState(0)->getCoordinates() <<"\n";
// }
//
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace FiniteVolume

  } // namespace Numerics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////
