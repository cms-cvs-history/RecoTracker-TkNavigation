#include "RecoTracker/TkNavigation/interface/SimpleNavigationSchool.h"
#include "RecoTracker/TkNavigation/interface/SimpleBarrelNavigableLayer.h"
#include "RecoTracker/TkNavigation/interface/SimpleForwardNavigableLayer.h"
#include "RecoTracker/TkNavigation/interface/SimpleNavigableLayer.h"
//#include "Tracker/TkLayout/interface/CmsTracker.h"
//#include "Tracker/TkLayout/interface/FullTracker.h"
#include "TrackingTools/DetLayers/interface/BarrelDetLayer.h"
#include "TrackingTools/DetLayers/interface/ForwardDetLayer.h"
#include "Geometry/Surface/interface/BoundCylinder.h"
#include "Geometry/Surface/interface/BoundDisk.h"

#include "Utilities/General/interface/CMSexception.h"
#include "TrackingTools/DetLayers/src/DetBelowZ.h"
#include "TrackingTools/DetLayers/src/DetLessZ.h"
#include "RecoTracker/TkNavigation/interface/DiskLessInnerRadius.h"
#include "RecoTracker/TkNavigation/interface/SymmetricLayerFinder.h"
#include "TrackingTools/DetLayers/interface/NavigationSetter.h"

#include <functional>
#include <algorithm>
#include <map>
#include <cmath>


//---- Temporary solution (CmsTrackerFromLayers class is needed)
class CmsTracker {
public:
  CmsTracker(){}
  vector<DetLayer*> barrelLayers(){return vector<DetLayer*>();}
  vector<DetLayer*> forwardLayers(){return vector<DetLayer*>();}
  vector<DetLayer*> allLayers(){return vector<DetLayer*>();}
};
//----

SimpleNavigationSchool::SimpleNavigationSchool(const MagneticField* field) : 
  theBarrelLength(0),theField(field)
{
  
  // Get barrel layers
  //vector<DetLayer*> blc = FullTracker::instance()->barrelLayers();  //TO BE FIXED...
  vector<DetLayer*> blc = CmsTracker().barrelLayers(); //TO BE FIXED...
  for ( vector<DetLayer*>::iterator i = blc.begin(); i != blc.end(); i++) {
    BarrelDetLayer* blp = dynamic_cast<BarrelDetLayer*>(*i);
    //if ( blp == 0) throw DetLogicError("Bad BarrelDetLayer");  //TO BE FIXED...
    if ( blp == 0) throw Genexception("Bad BarrelDetLayer");
    theBarrelLayers.push_back( blp);
  }

  // get forward layers
  //vector<DetLayer*> flc = FullTracker::instance()->forwardLayers();  //TO BE FIXED...
  vector<DetLayer*> flc = CmsTracker().forwardLayers(); //TO BE FIXED...
  {
  for ( vector<DetLayer*>::iterator i = flc.begin(); i != flc.end(); i++) {
    ForwardDetLayer* flp = dynamic_cast<ForwardDetLayer*>(*i);
    //if ( flp == 0) throw DetLogicError("Bad ForwardDetLayer");  //TO BE FIXED...
    if ( flp == 0) throw Genexception("Bad BarrelDetLayer");
    theForwardLayers.push_back( flp);
  }
  }
  FDLI middle = find_if( theForwardLayers.begin(), theForwardLayers.end(),
			 not1(DetBelowZ(0)));
  theLeftLayers  = FDLC( theForwardLayers.begin(), middle);
  theRightLayers = FDLC( middle, theForwardLayers.end());
  
  SymmetricLayerFinder symFinder( theForwardLayers);

  // only work on positive Z side; negative by mirror symmetry later
  linkBarrelLayers( symFinder);
  linkForwardLayers( symFinder);
  establishInverseRelations();
}

SimpleNavigationSchool::StateType 
SimpleNavigationSchool::navigableLayers() const
{
  StateType result;
  for ( vector< SimpleBarrelNavigableLayer*>::const_iterator 
	  ib = theBarrelNLC.begin(); ib != theBarrelNLC.end(); ib++) {
    result.push_back( *ib);
  }
  for ( vector< SimpleForwardNavigableLayer*>::const_iterator 
	  ifl = theForwardNLC.begin(); ifl != theForwardNLC.end(); ifl++) {
    result.push_back( *ifl);
  }
  return result;
}

void SimpleNavigationSchool::
linkBarrelLayers( SymmetricLayerFinder& symFinder)
{
  // Link barrel layers outwards
  for ( BDLI i = theBarrelLayers.begin(); i != theBarrelLayers.end(); i++) {
    BDLC reachableBL;
    FDLC leftFL;
    FDLC rightFL;

    // always add next barrel layer first
    if ( i+1 != theBarrelLayers.end()) reachableBL.push_back(*(i+1));
 
    // Add closest reachable forward layer (except for last BarrelLayer)
    if (i != theBarrelLayers.end() - 1) {
      linkNextForwardLayer( *i, rightFL);
    }

    // Add next BarrelLayer with length larger than the current BL
    if ( i+2 < theBarrelLayers.end()) {
      linkNextLargerLayer( i, theBarrelLayers.end(), reachableBL);
    }

    theBarrelNLC.push_back( new 
       SimpleBarrelNavigableLayer( *i, reachableBL,
				   symFinder.mirror(rightFL),
				   rightFL,theField, 5.));
  }
}

void SimpleNavigationSchool::linkNextForwardLayer( BarrelDetLayer* bl, 
						   FDLC& rightFL)
{
  // find first forward layer with larger Z and larger outer radius
  float length = bl->surface().bounds().length() / 2.;
  float radius = bl->specificSurface().radius();
  for ( FDLI fli = theRightLayers.begin();
	fli != theRightLayers.end(); fli++) {
    if ( length < (**fli).position().z() &&
	 radius < (**fli).specificSurface().outerRadius()) {
      rightFL.push_back( *fli);
      return;
    }
  }
}

void SimpleNavigationSchool::linkNextLargerLayer( BDLI bli, BDLI end,
						  BDLC& reachableBL)
{
  // compare length of next layer with length of following ones
  float length = (**(bli+1)).surface().bounds().length();
  float epsilon = 0.1;

  for ( BDLI i = bli+2; i < end; i++) {
    if ( length + epsilon < (**i).surface().bounds().length()) {
      reachableBL.push_back( *i);
      return;
    }
  }
}

void SimpleNavigationSchool::
linkForwardLayers( SymmetricLayerFinder& symFinder)
{

  // handle right side first, groups are only on the right 
  vector<FDLC> groups = splitForwardLayers();

  cout << " Forward groups size = " << groups.size() << endl;
  for (vector<FDLC>::iterator g = groups.begin(); g != groups.end(); g++) {
    cout << "group " << g - groups.begin() << " has " 
	 << g->size() << " layers " << endl;
  }

  for ( vector<FDLC>::iterator group = groups.begin();
	group != groups.end(); group++) {

    for ( FDLI i = group->begin(); i != group->end(); i++) {

      BDLC reachableBL;
      FDLC reachableFL;
 
      // Always connect to next barrel layer first, if exists
      linkNextBarrelLayer( *i, reachableBL);

      // Then always connect to next forward layer of "same" size, 
      // and layers of larger inner Radius
      linkNextLayerInGroup( i, *group, reachableFL);

      // Then connect to next N fw layers of next size
      if ( group+1 != groups.end()) {
	linkOuterGroup( *i, *(group+1), reachableFL);
      }

      // or connect within the group if outer radius increases
      linkWithinGroup( i, *group, reachableFL);

      theForwardNLC.push_back( new SimpleForwardNavigableLayer( *i,reachableBL,
								reachableFL,
								theField,
								5.));
      theForwardNLC.push_back( new SimpleForwardNavigableLayer( symFinder.mirror(*i),
								reachableBL,
								symFinder.mirror(reachableFL),
								theField,
								5.));

    }
  }

//    // now the left side by symmetry
//    for ( FDLI ileft = theLeftLayers.begin(); 
//  	ileft != theLeftLayers.end(); ileft++) {
//      ForwardDetLayer* right = symFinder.mirror( *ileft);
    
//      theForwardNLC.push_back( new 
//         SimpleForwardNavigableLayer( *ileft , right->nextBarrelLayers(),
//  	                      symFinder.mirror(right->nextForwardLayers())));
//    }
}

void SimpleNavigationSchool::linkNextBarrelLayer( ForwardDetLayer* fl,
						  BDLC& reachableBL)
{
  if ( fl->position().z() > barrelLength()) return;

  float outerRadius = fl->specificSurface().outerRadius();
  float zpos        = fl->position().z();
  for ( BDLI bli = theBarrelLayers.begin(); bli != theBarrelLayers.end(); bli++) {
    if ( outerRadius < (**bli).specificSurface().radius() &&
	 zpos        < (**bli).surface().bounds().length() / 2.) {
      reachableBL.push_back( *bli);
      return;
    }
  }
}


void SimpleNavigationSchool::linkNextLayerInGroup( FDLI fli,
						   const FDLC& group,
						   FDLC& reachableFL)
{
  // Always connect to next forward layer of "same" size, if exists
  if ( fli+1 != group.end()) {
    reachableFL.push_back( *(fli+1));
    // If that layer has an inner radius larger then the current one
    // also connect ALL next disks of same radius.
    float innerRThis = (**fli).specificSurface().innerRadius();
    float innerRNext =  (**(fli+1)).specificSurface().innerRadius();
    const float epsilon = 2.f;

    if (innerRNext > innerRThis + epsilon) {
      // next disk is smaller, so it doesn't cover fully subsequent ones
      // of same radius

      int i = 2;
      while ( (fli+i) != group.end()) {
	if ( (**(fli+i)).specificSurface().innerRadius() < 
	     innerRNext + epsilon) {
	  // following disk has not increased in ineer radius 
	  reachableFL.push_back( *(fli+i));
	  i++;
	} else {
	  break;
	}
      }
    }
  }
}


void SimpleNavigationSchool::linkOuterGroup( ForwardDetLayer* fl,
					     const FDLC& group,
					     FDLC& reachableFL)
{

  // insert N layers with Z grater than fl

  ConstFDLI first = find_if( group.begin(), group.end(), 
			     not1( DetBelowZ( fl->position().z())));
  if ( first != group.end()) {

    // Hard-wired constant!!!!!!
    ConstFDLI last = min( first + 7, group.end());

    reachableFL.insert( reachableFL.end(), first, last);
  }
}

void SimpleNavigationSchool::linkWithinGroup( FDLI fl,
					      const FDLC& group,
					      FDLC& reachableFL)
{
  ConstFDLI biggerLayer = outerRadiusIncrease( fl, group);
  if ( biggerLayer != group.end() && biggerLayer != fl+1) {
    reachableFL.push_back( *biggerLayer);
  }
}

SimpleNavigationSchool::ConstFDLI
SimpleNavigationSchool::outerRadiusIncrease( FDLI fl, const FDLC& group)
{
  const float epsilon = 5.f;
  float outerRadius = (**fl).specificSurface().outerRadius();
  while ( ++fl != group.end()) {
    if ( (**fl).specificSurface().outerRadius() > outerRadius + epsilon) {
      return fl;
    }
  }
  return fl;
}

vector<SimpleNavigationSchool::FDLC> 
SimpleNavigationSchool::splitForwardLayers() 
{
  // only work on positive Z side; negative by mirror symmetry later

  FDLC myRightLayers( theRightLayers);
  FDLI begin = myRightLayers.begin();
  FDLI end   = myRightLayers.end();

  // sort according to inner radius
  sort ( begin, end, DiskLessInnerRadius()); 

  // partition in cylinders
  vector<FDLC> result;
  FDLC current;
  current.push_back( *begin);
  for ( FDLI i = begin+1; i != end; i++) {

    cout << "(**i).specificSurface().innerRadius()      = "
	 << (**i).specificSurface().innerRadius() << endl
	 << "(**(i-1)).specificSurface().outerRadius()) = "
	 << (**(i-1)).specificSurface().outerRadius() << endl;

    // if inner radius of i is larger than outer radius of i-1 then split!
    if ( (**i).specificSurface().innerRadius() > 
	 (**(i-1)).specificSurface().outerRadius()) {

      cout << "found break between groups" << endl;

      // sort layers in group along Z
      sort ( current.begin(), current.end(), DetLessZ());

      result.push_back(current);
      current.clear();
    }
    current.push_back(*i);
  }
  result.push_back(current); // save last one too 

  // now sort subsets in Z
  for ( vector<FDLC>::iterator ivec = result.begin();
	ivec != result.end(); ivec++) {
    sort( ivec->begin(), ivec->end(), DetLessZ());
  }

  return result;
}

float SimpleNavigationSchool::barrelLength() 
{
  if ( theBarrelLength < 1.) {
    for (BDLI i=theBarrelLayers.begin(); i!=theBarrelLayers.end(); i++) {
      theBarrelLength = max( theBarrelLength,
			     (**i).surface().bounds().length() / 2.f);
    }

    cout << "The barrel length is " << theBarrelLength << endl;
  }
  return theBarrelLength;
}

void SimpleNavigationSchool::establishInverseRelations() {

  NavigationSetter setter(*this);

    // find for each layer which are the barrel and forward
    // layers that point to it
    typedef map<const DetLayer*, vector<BarrelDetLayer*>, less<const DetLayer*> > BarrelMapType;
    typedef map<const DetLayer*, vector<ForwardDetLayer*>, less<const DetLayer*> > ForwardMapType;


    BarrelMapType reachedBarrelLayersMap;
    ForwardMapType reachedForwardLayersMap;


    for ( BDLI bli = theBarrelLayers.begin();
        bli!=theBarrelLayers.end(); bli++) {
      DLC reachedLC = (**bli).nextLayers( alongMomentum);
      for ( DLI i = reachedLC.begin(); i != reachedLC.end(); i++) {
        reachedBarrelLayersMap[*i].push_back( *bli);
      }
    }

    for ( FDLI fli = theForwardLayers.begin();
        fli!=theForwardLayers.end(); fli++) {
      DLC reachedLC = (**fli).nextLayers( alongMomentum);
      for ( DLI i = reachedLC.begin(); i != reachedLC.end(); i++) {
        reachedForwardLayersMap[*i].push_back( *fli);
      }
    }


    //vector<DetLayer*> lc = FullTracker::instance()->allLayers(); //TO BE FIXED...
    vector<DetLayer*> lc = CmsTracker().allLayers();              //TO BE FIXED...
  for ( vector<DetLayer*>::iterator i = lc.begin(); i != lc.end(); i++) {
    SimpleNavigableLayer* navigableLayer =
      dynamic_cast<SimpleNavigableLayer*>((**i).navigableLayer());
    navigableLayer->setInwardLinks( reachedBarrelLayersMap[*i],reachedForwardLayersMap[*i] );
  }

}
