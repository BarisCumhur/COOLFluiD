#include "Common/PE.hh"
#include "MathTools/MathConsts.hh"
#include "MathTools/MatrixInverterT.hh"
#include "Environment/FileHandlerOutput.hh"
#include "Environment/DirPaths.hh"
#include "Environment/SingleBehaviorFactory.hh"
#include "Common/BadValueException.hh"
#include "Framework/PathAppender.hh"
#include "Framework/DataProcessing.hh"
#include "Framework/SubSystemStatus.hh"
#include "Framework/MethodCommandProvider.hh"
#include "Framework/MeshData.hh"
#include "FiniteVolume/CellCenterFVM.hh"
#include "FiniteVolumeICP/FiniteVolumeICP.hh"
#include "FiniteVolumeICP/StagnationPropsBL.hh"

#include <iostream>
#include <fstream>
#include <sstream>

/////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;
using namespace COOLFluiD::Numerics::FiniteVolume;
using namespace COOLFluiD::MathTools;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

    namespace Numerics {

  namespace FiniteVolumeICP {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<StagnationPropsBL, DataProcessingData, FiniteVolumeICPModule>
StagnationPropsBLCommProvider("StagnationPropsBLComm");

//////////////////////////////////////////////////////////////////////////////

void StagnationPropsBL::defineConfigOptions(Config::OptionList& options)
{
  options.addConfigOption< CFreal >("TorchExitXCoord","X coordinate where torch ends and chamber starts (in mesh coordinates)");
  options.addConfigOption< CFreal >("ProbeRadius","Radiusof the probe (in mesh coordinates)");
}

//////////////////////////////////////////////////////////////////////////////

StagnationPropsBL::StagnationPropsBL(const std::string& name) :
  DataProcessingCom(name),
  socket_states("states"),
  m_xmax(0.),
  m_localstagnationline(0),
  m_globalstagnationline(0),
  m_nrcv(0),
  m_dspl(0),
  m_torchexitxcoord(0.486),
  m_proberadius(0.050)
{
  addConfigOptionsTo(this);
  setParameter("TorchExitXCoord",&m_torchexitxcoord);
  setParameter("ProbeRadius",&m_proberadius);
}

//////////////////////////////////////////////////////////////////////////////

StagnationPropsBL::~StagnationPropsBL()
{
}

//////////////////////////////////////////////////////////////////////////////

std::vector<Common::SafePtr<BaseDataSocketSource> >
StagnationPropsBL::providesSockets()
{
  std::vector<Common::SafePtr<BaseDataSocketSource> > result;
  return result;
}

//////////////////////////////////////////////////////////////////////////////

std::vector<Common::SafePtr<BaseDataSocketSink> >
StagnationPropsBL::needsSockets()
{
  std::vector<Common::SafePtr<BaseDataSocketSink> > result;
  result.push_back(&socket_states);
  return result;
}

//////////////////////////////////////////////////////////////////////////////

void StagnationPropsBL::setup()
{
  CFAUTOTRACE;
  const CFreal TOL=1e-8;
  const CFuint irank=PE::GetPE().GetRank();
  const CFuint nproc=PE::GetPE().GetProcessorCount();

  CFout << "Setup: ComputingBL stagnation properties.\n";

  DataHandle < Framework::State*, Framework::GLOBAL > states = socket_states.getDataHandle();

  // builder for standard TRS GeometricEntity's that will be used
  Framework::GeometricEntityPool<Framework::StdTrsGeoBuilder> geoBuilder;
  geoBuilder.setup();
  StdTrsGeoBuilder::GeoData& geoData = geoBuilder.getDataGE();
  geoData.trs = getCurrentTRS();
  CFuint nbCells = getCurrentTRS()->getLocalNbGeoEnts();

  // cell loop to contribute
  double xmax=-1e30;
  for (CFuint iCell = 0; iCell < nbCells; ++iCell)
  {
    geoData.idx = iCell;
    GeometricEntity & currCell = *geoBuilder.buildGE();

    for(int iNode=0; iNode<(const int)currCell.nbNodes(); ++iNode)
      if (std::abs((*(currCell.getNode(iNode)->getData()))[1])<TOL)
        if ((states[iCell])->isParUpdatable())
        {
          CFreal xcoord=0.;
          for(int jNode=0; jNode<(const int)currCell.nbNodes(); ++jNode)
            xcoord+=(*(currCell.getNode(jNode)->getData()))[0];
          xcoord/=(CFreal)currCell.nbNodes();
          m_localstagnationline.push_back(std::make_pair(xcoord,iCell));
          //m_localstagnationline.push_back(std::make_pair((*(currCell.getNode(iNode)->getData()))[0],iCell));
          break;
        }
    for(int iNode=0; iNode<(const int)currCell.nbNodes(); ++iNode)
      if (std::abs((*(currCell.getNode(iNode)->getData()))[1])<TOL)
        xmax=(*(currCell.getNode(iNode)->getData()))[0]>xmax?(*(currCell.getNode(iNode)->getData()))[0]:xmax;

    geoBuilder.releaseGE();
  }

  // xmax global
  CFreal buf=xmax;
  xmax=0.;
  MPI_Allreduce(&buf,&xmax,1,MPI_DOUBLE,MPI_MAX,PE::GetPE().GetCommunicator());
  m_xmax=xmax;

  // transform
  for(int i=0; i<(const int)m_localstagnationline.size(); ++i)
    m_localstagnationline[i].first=xmax-m_localstagnationline[i].first;

  // build info for gatherv
  m_localstagnationline.reserve(m_localstagnationline.size()+1); // to avoid segfault when no nodes on process
  int nirank=(int)m_localstagnationline.size();
  m_nrcv.reserve(nproc); // to avoid segfault when no nodes on process
  if (irank==0) m_nrcv.resize(nproc,0);
  MPI_Gather(&nirank, 1, MPI_INT, &m_nrcv[0], 1, MPI_INT, 0, PE::GetPE().GetCommunicator());
  int sumelms=0;
  m_dspl.reserve(nproc); // to avoid segfault when no nodes on process
  if (irank==0) {
    m_dspl.resize(nproc,0);
    sumelms=m_nrcv[0];
    for (int i=1; i<(const int)nproc; ++i)
    {
      m_dspl[i]=m_dspl[i-1]+m_nrcv[i-1];
      sumelms+=m_nrcv[i];
    }
  }

  // collect coordinates
  m_nrcv.reserve(sumelms+1); // to avoid segfault when no nodes on process
  if (irank==0) m_nrcv.resize(sumelms,0.);
  std::vector<CFreal> transcoord(nirank);
  for (int i=0; i<nirank; ++i) transcoord[i]=m_localstagnationline[i].first;
  std::vector<CFreal> rcvcoord(sumelms);
  MPI_Gatherv(&transcoord[0],nirank, MPI_DOUBLE, &rcvcoord[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());

  if (irank==0) {
    m_globalstagnationline.reserve(sumelms);
    for(int i=0; i<sumelms; ++i)
      m_globalstagnationline.push_back(std::make_pair(rcvcoord[i],i));
    std::sort(m_globalstagnationline.begin(),m_globalstagnationline.end());
  }
}

//////////////////////////////////////////////////////////////////////////////

void StagnationPropsBL::execute()
{
  CFAUTOTRACE;

  const CFuint irank=PE::GetPE().GetRank();
  DataHandle < Framework::State*, Framework::GLOBAL > states = socket_states.getDataHandle();

  // fill local vectors with x and y component of velocity
  std::vector<CFreal> plocal(m_localstagnationline.size());
  std::vector<CFreal> ulocal(m_localstagnationline.size());
  std::vector<CFreal> vlocal(m_localstagnationline.size());
  std::vector<CFreal> tlocal(m_localstagnationline.size());
  std::vector<CFreal> ylocal(m_localstagnationline.size());
  std::vector<CFreal> dvdylocal(m_localstagnationline.size());

  for(int i=0; i<(const int)m_localstagnationline.size(); ++i){
    State& s= *states[m_localstagnationline[i].second];
    Node n=s.getCoordinates();
    plocal[i]=s[0];
    ulocal[i]=s[1];
    vlocal[i]=s[2];
    tlocal[i]=s[3];
    ylocal[i]=n[1];
    dvdylocal[i]=s[2]/n[1];
  }

  // collect to process zero
  std::vector<CFreal> pg(m_globalstagnationline.size());
  std::vector<CFreal> ug(m_globalstagnationline.size());
  std::vector<CFreal> vg(m_globalstagnationline.size());
  std::vector<CFreal> tg(m_globalstagnationline.size());
  std::vector<CFreal> yg(m_globalstagnationline.size());
  std::vector<CFreal> dvdyg(m_globalstagnationline.size());
  std::vector<CFreal> pglobal(m_globalstagnationline.size());
  std::vector<CFreal> uglobal(m_globalstagnationline.size());
  std::vector<CFreal> vglobal(m_globalstagnationline.size());
  std::vector<CFreal> tglobal(m_globalstagnationline.size());
  std::vector<CFreal> yglobal(m_globalstagnationline.size());
  std::vector<CFreal> dvdyglobal(m_globalstagnationline.size());
  MPI_Gatherv(&plocal[0],(int)plocal.size(), MPI_DOUBLE, &pg[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());
  MPI_Gatherv(&ulocal[0],(int)ulocal.size(), MPI_DOUBLE, &ug[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());
  MPI_Gatherv(&vlocal[0],(int)vlocal.size(), MPI_DOUBLE, &vg[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());
  MPI_Gatherv(&tlocal[0],(int)tlocal.size(), MPI_DOUBLE, &tg[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());
  MPI_Gatherv(&ylocal[0],(int)ylocal.size(), MPI_DOUBLE, &yg[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());
  MPI_Gatherv(&dvdylocal[0],(int)dvdylocal.size(), MPI_DOUBLE, &dvdyg[0] ,&m_nrcv[0], &m_dspl[0], MPI_DOUBLE,0,PE::GetPE().GetCommunicator());

  // move to ordered by X
  if (irank==0){
    for (int i=0; i<(const int)m_globalstagnationline.size(); ++i){
      pglobal[i]=pg[m_globalstagnationline[i].second];
      uglobal[i]=ug[m_globalstagnationline[i].second];
      vglobal[i]=vg[m_globalstagnationline[i].second];
      tglobal[i]=tg[m_globalstagnationline[i].second];
      yglobal[i]=yg[m_globalstagnationline[i].second];
      dvdyglobal[i]=dvdyg[m_globalstagnationline[i].second];
    }
  }

  // compute dvdx
  std::vector<CFreal> d2vdydx(m_globalstagnationline.size(),0.);
  std::vector<CFreal> dudx(m_globalstagnationline.size(),0.);  
  if (irank==0)
  {
      for(int i=1; i<(const int)(d2vdydx.size()-1); ++i) {
      d2vdydx[i]=(dvdyglobal[i+1]-dvdyglobal[i-1])/(m_globalstagnationline[i+1].first-m_globalstagnationline[i-1].first);
      dudx[i]=(uglobal[i+1]-uglobal[i-1])/(m_globalstagnationline[i+1].first-m_globalstagnationline[i-1].first);
      }

    d2vdydx[0]=d2vdydx[1];
    d2vdydx[d2vdydx.size()-1]=d2vdydx[d2vdydx.size()-2];

      dudx[0]=dudx[1];
      dudx[dudx.size()-1]=dudx[dudx.size()-2];
  }

  // 1. by knowing that the function shape, assuming that before the inflexion point dvdx is always positive
  //    therefore looking for the first minimum of dvdx instead of d2vdx2=0, numerically more reliable
  // 2. minimal point found, then using the two neighbouring points, building up the coeffs of an equation of a parabola
  //    and then the minimum can be found analytically
  std::vector<CFreal> inflex_interp(m_globalstagnationline.size(),0.);
  CFreal delta=0.;
  CFreal y_delta=0.;
  CFreal p_delta=0.;
  CFreal u_delta=0.;
  CFreal v_delta=0.;
  CFreal t_delta=0.;
  CFreal dvdy_delta=0.;
  CFreal d2vdydx_delta=0.;
  
  CFreal   dudxMax      = 0.0;
  CFreal   uAtDudxMax   = 0.0;
  unsigned iDudxMax     = 0; 

  if (irank==0)
  {
    // skipping first 5 points, because BL may be oscillatory
    for(int i=5; i<(const int)(d2vdydx.size()-5); ++i)
      if ((d2vdydx[i]-d2vdydx[i-1]<=0.)&&(d2vdydx[i+1]-d2vdydx[i]>0.))
      {
        // a2*x*x+a1*x+a0=0
        RealMatrix m(3,3);
        RealMatrix invm(3,3);
        RealVector a(3);
        RealVector f(3);
        m(0,0)=1.; m(0,1)=m_globalstagnationline[i-1].first; m(0,2)=m(0,1)*m(0,1);
        m(1,0)=1.; m(1,1)=m_globalstagnationline[i].first;   m(1,2)=m(1,1)*m(1,1);
        m(2,0)=1.; m(2,1)=m_globalstagnationline[i+1].first; m(2,2)=m(2,1)*m(2,1);
        f[0]=d2vdydx[i-1];
        f[1]=d2vdydx[i];
        f[2]=d2vdydx[i+1];
        MathTools::MatrixInverterT<3> inverter;
        inverter.invert(m, invm);
        a=invm*f;

        // 2*a2*x+a1=0 tells the X coord where minimum
        const CFreal minX=-a[1]/2./a[2];
        const CFreal dxm1=std::abs(minX-m_globalstagnationline[i-1].first);
        const CFreal dxcc=std::abs(minX-m_globalstagnationline[i].first);
        const CFreal dxp1=std::abs(minX-m_globalstagnationline[i+1].first);
        inflex_interp[i-1]=dxm1/(dxm1+dxcc+dxp1);
        inflex_interp[i]  =dxcc/(dxm1+dxcc+dxp1);
        inflex_interp[i+1]=dxp1/(dxm1+dxcc+dxp1);

        // computing the params
        delta=minX;
        d2vdydx_delta=a[2]*minX*minX+a[1]*minX+a[0];

        f[0]=dvdyglobal[i-1];
        f[1]=dvdyglobal[i];
        f[2]=dvdyglobal[i+1];
        a=invm*f;
        dvdy_delta=a[2]*minX*minX+a[1]*minX+a[0];

        f[0]=pglobal[i-1];
        f[1]=pglobal[i];
        f[2]=pglobal[i+1];
        a=invm*f;
        p_delta=a[2]*minX*minX+a[1]*minX+a[0];

        f[0]=uglobal[i-1];
        f[1]=uglobal[i];
        f[2]=uglobal[i+1];
        a=invm*f;
        u_delta=a[2]*minX*minX+a[1]*minX+a[0];

        f[0]=vglobal[i-1];
        f[1]=vglobal[i];
        f[2]=vglobal[i+1];
        a=invm*f;
        v_delta=a[2]*minX*minX+a[1]*minX+a[0];

        f[0]=tglobal[i-1];
        f[1]=tglobal[i];
        f[2]=tglobal[i+1];
        a=invm*f;
        t_delta=a[2]*minX*minX+a[1]*minX+a[0];

        f[0]=yglobal[i-1];
        f[1]=yglobal[i];
        f[2]=yglobal[i+1];
        a=invm*f;
        y_delta=a[2]*minX*minX+a[1]*minX+a[0];

        break;
      }

    // attempt to compute the u velocity at the point where the du/dx derivative is minimum to find
    // approximate value of the free stream velocity in case there is no probe
    // we limit the earch between x=[0.01-0.4], where the probe is at x=0.
    
    // skipping first 5 points, because BL may be oscillatory
    for(int i=5; i<(const int)(dudx.size()-5); ++i){
        if ((dudx[i]-dudx[i-1]>=0.)&&(m_globalstagnationline[i-1].first > 0.01)&&(m_globalstagnationline[i-1].first < 0.4)){
            iDudxMax = i-1;
            dudxMax  = dudx[i-1];
            uAtDudxMax = uglobal[i-1];  
//             CFout << "\n Found U at max du/dx!   i        =  " << iDudxMax        
//                   << "\n du/dx at i-2                     =  " << dudx[i-2]     
//                   << "\n du/dx at i-1                     =  " << dudx[i-1]  
//                   << "\n du/dx at i                       =  " << dudx[i] 
//                   << "\n du/dx at i+1                     =  " << dudx[i+1] 
//                   << "\n du/dx at i+2                     =  " << dudx[i+2] 
//                   << "\n                      du/dx max   =  " << dudxMax   
//                   << "\n                 U at du/dx max   =  " << uAtDudxMax ;
            break;
        }

    }

    if ( (iDudxMax == 0) || (std::abs(uAtDudxMax) < 1.0e-6 )) {
        cout << " Warning!! du/dx max calculation probably failed!  i      =  " << iDudxMax << endl
             << "                      du/dx max =  " << dudxMax << endl
             << "                 U at du/dx max =  " << uAtDudxMax << endl;
            
    }

    
    
  }

  // look up velocity at torch exit
  CFreal xtorchexit=0.;
  CFreal utorchexit=0.;
  if (irank==0)
  {
    for(int i=0; i<(const int)(m_globalstagnationline.size()-1); ++i)
      if ((m_xmax-m_globalstagnationline[i].first>=m_torchexitxcoord)&&(m_xmax-m_globalstagnationline[i+1].first<=m_torchexitxcoord)){
        CFreal alpha=(m_torchexitxcoord-(m_xmax-m_globalstagnationline[i+1].first))/(m_globalstagnationline[i+1].first-m_globalstagnationline[i].first);
        xtorchexit=alpha*m_globalstagnationline[i].first+(1.-alpha)*m_globalstagnationline[i+1].first;
        utorchexit=alpha*uglobal[i]+(1.-alpha)*uglobal[i+1];

//CFout << "torchexit:"
//      << "\n  torchexitxcoord " << m_torchexitxcoord
//      << "\n  X[i]   " << m_globalstagnationline[i].first
//      << "\n  X[i+1] " << m_globalstagnationline[i+1].first
//      << "\n  U[i]   " << uglobal[i]
//      << "\n  U[i+1] " << uglobal[i+1]
//      << "\n  xmax   " << m_xmax
//      << "\n  alpha  " << alpha
//      << "\n";

        break;
      }
  }

  // write a cross-check tecplot file
  if (irank==0){
    boost::filesystem::path fpath = Environment::DirPaths::getInstance().getResultsDir() / boost::filesystem::path ("stagnationpropsBL.plt");
    fpath = PathAppender::getInstance().appendAllInfo(fpath, false, false, false );
    SelfRegistPtr<Environment::FileHandlerOutput> fhandle = Environment::SingleBehaviorFactory<Environment::FileHandlerOutput>::getInstance().create();
    ofstream& fout = fhandle->open(fpath);
    fout.precision(15);
    fout << "VARIABLES = x y p u v T dvdy d2vdydx dudx inflexion" << "\n" << flush;
    fout << scientific
         << "0."
         << " " << yglobal[0]
         << " " << pglobal[0]
         << " 0. 0."
         << " " << tglobal[0]
         << " " << dvdyglobal[0]
         << " " << d2vdydx[0]
         << " " << dudx[0]
         << " " << inflex_interp[0]
         << "\n";
    for (int i=0; i<(const int)m_globalstagnationline.size(); ++i){
      fout << scientific << m_globalstagnationline[i].first << " "
                         << yglobal[i]       << " "
                         << pglobal[i]       << " "
                         << uglobal[i]       << " "
                         << vglobal[i]       << " "
                         << tglobal[i]       << " "
                         << dvdyglobal[i]    << " "
                         << d2vdydx[i]       << " "
                         << dudx[i]          << " "
                         << inflex_interp[i] << " "
      << "\n" << flush;
    }
    fhandle->close();


    // Also write the NDOP's on a file
    boost::filesystem::path fpath2 = Environment::DirPaths::getInstance().getResultsDir() / boost::filesystem::path ("NDP.dat");
    fpath2 = PathAppender::getInstance().appendAllInfo(fpath2, false, false, false );
    SelfRegistPtr<Environment::FileHandlerOutput> fhandle2 = Environment::SingleBehaviorFactory<Environment::FileHandlerOutput>::getInstance().create();
    ofstream& fout2 = fhandle2->open(fpath2);

    /*
      Note here, Tamas's convention of (u,v) and (x,y):

      v,y 
                                 | delta
      ^                          |<--->|
      |                          |
      |                          |       ____________
      |                          |      /         ^
      |                          |      | Probe   | Probe Radius
      |------------------------- |------|-----------> u,x
     */
                                 
    CFreal ndp1,ndp2,ndp3,ndp4,ndp5,beta,dbeta_dx;
    beta    = dvdy_delta;
    dbeta_dx=std::abs(d2vdydx_delta);
    ndp1    = delta/m_proberadius;
    ndp2    = beta*m_proberadius/utorchexit;
    ndp3    = dbeta_dx*m_proberadius*m_proberadius/utorchexit;
    ndp4    = u_delta/utorchexit;
    ndp5    = u_delta/uAtDudxMax;

    fout2.precision(15);
    fout2  << ndp1 << "  " << ndp2 << "  " << ndp3 << "  " << ndp4 << "  " << ndp5 << " "
           << m_proberadius << "  " << delta << "  " <<  u_delta << "  " << beta << "  "
           << dbeta_dx << "  " << utorchexit << "  " << uAtDudxMax << "\n";
    fhandle2->close();
    
    CFout << "\n\nStagnationPropsBL:  "
          << "\n  ndp1.................:  " << ndp1
          << "\n  ndp2.................:  " << ndp2
          << "\n  ndp3.................:  " << ndp3
          << "\n  ndp4.................:  " << ndp4
          << "\n  ndp5.................:  " << ndp5
          << "\n  Probe Radius.........:  " << m_proberadius
          << "\n  delta................:  " << delta
          << "\n  y@delta..............:  " << y_delta
          << "\n  p@delta..............:  " << p_delta 
          << "\n  u@delta..............:  " << u_delta
          << "\n  v@delta..............:  " << v_delta
          << "\n  T@delta..............:  " << t_delta
          << "\n  beta==dv/dy..........:  " << beta
          << "\n  dbeta/dx.............:  " << dbeta_dx
          << "\n  xtorchexit...........:  " << xtorchexit
          << "\n  u@xtorchexit.........:  " << utorchexit
          << "\n  iDudxMax.............:  " << iDudxMax
          << "\n  du/dx max........... :  " << dudxMax
          << "\n  U at du/dx max...... :  " << uAtDudxMax
          << "\n\n";
        
  }

}

//////////////////////////////////////////////////////////////////////////////

void StagnationPropsBL::unsetup()
{
  CFAUTOTRACE;
}

//////////////////////////////////////////////////////////////////////////////

    } // namespace FiniteVolumeICP

  } // namespace Numerics

} // namespace COOLFluiD

//////////////////////////////////////////////////////////////////////////////

