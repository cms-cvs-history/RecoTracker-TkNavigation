#include "RecoTracker/TkNavigation/interface/NavigationSchoolFactory.h"

#include "FWCore/PluginManager/interface/PluginFactory.h"
 
//EDM_REGISTER_PLUGINFACTORY(NavigationSchoolFactory,"NavigationSchoolFactory");
NavigationSchoolFactory::NavigationSchoolFactory()
  : seal::PluginFactory<NavigationSchool *(const GeometricSearchTracker* theTracker,const MagneticField* field)>("NavigationSchoolFactory")
{ }

NavigationSchoolFactory::~NavigationSchoolFactory()
{ }

NavigationSchoolFactory * NavigationSchoolFactory::get()
{
  static NavigationSchoolFactory theNavigationSchoolFactory;
  return & theNavigationSchoolFactory;
}
