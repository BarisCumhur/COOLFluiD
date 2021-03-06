#include "RemeshMeandr/RemeshMeandr.hh"
#include "StdUnSetup.hh"
#include "Framework/MethodCommandProvider.hh"
#include "Framework/MeshData.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace COOLFluiD::Framework;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

  namespace Numerics {

    namespace RemeshMeandros {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider<StdUnSetup, RMeshMeData, RemeshMeandrModule> stdUnSetupProvider("StdUnSetup");

//////////////////////////////////////////////////////////////////////////////

void StdUnSetup::execute()
{

}

//////////////////////////////////////////////////////////////////////////////

    } // namespace RemeshMeandros

  } // namespace Numerics

} // namespace COOLFluiD
