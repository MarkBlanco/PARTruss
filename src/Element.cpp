// Implementation of class for members in a truss
// NOTE: Only implementing stiffness method for 2D (for now?)!

#ifndef ELEM
#include "Element.hpp"
#define ELEM
#endif

extern int DEBUGLVL;
extern int COMMENTARY;

#include "util.hpp"

Element::Element ( Node & start, Node & end, double area, double E )
{
  _startNode = &start;
  _endNode = &end;
  _sectionArea = area;
  _youngModulus = E;
  calcElem();
}


Element::Element()
{
  _startNode = NULL;
  _endNode = NULL;
  _sectionArea = 0;
  _youngModulus = 0;
}


void Element::setElem ( Node & start, Node & end, double section, double E )
{
  this->_startNode = &start;
  this->_endNode = &end;
  this->_sectionArea = section;
  this->_youngModulus = E;
  calcElem();
}

void Element::calcElem()
{
  // Additional processing and setting of members not directly from args:
  std::valarray<double> coord1 ( getStart()->getCoords() );
  std::valarray<double> coord2 ( getEnd()->getCoords() );
  std::valarray<double> diffVec = coord1 - coord2;
  this->_length = std::pow ( diffVec, 2.0 ).sum();
  this->_weight = _sectionArea * _length * _density;
  this->_K = _sectionArea * _youngModulus / _length;  // Stiffness calculation
  this->_XYZRatio = diffVec / _length; // Ratio of x,y,z deltas to length
  // Assign congruent transformation matrix as dot product of transpose of
  //_XYZRatio with itself:
  double cx = this->_XYZRatio[0];
  double cy = this->_XYZRatio[1];
  double cz = this->_XYZRatio[2];
  double K = this->_K;
  double congruent_transformation[3][3] =
  { // Row-major format
    { K * pow ( cx, 2 ), K*cy * cx, K*cz * cx },
    { K*cx * cy, K * pow ( cy, 2 ), K*cz * cy },
    { K*cx * cz, K*cy * cz, K * pow ( cz, 2 ) }
  };
  // Now create local stiffness by tiling congruent transformation in the
  //following pattern:
  //
  //       1 -1
  //      -1  1
  //
  for ( int i = 0; i < 36; i ++ )
  {
    this->_localStiffness[i] = 0;
  }
  for ( int i = 0; i < 6; i ++ )
  {
    for ( int j = 0; j < 6; j++ )
    {
      double mult = ( ( i < 3 && j < 3 ) || ( i > 2 && j > 2 ) ) ? ( 1.0 ) : ( -1.0 );
      this->_localStiffness[i * 6 + j] = mult * congruent_transformation[i % 3][j % 3];
    }
  }
  if ( DEBUGLVL > 1 )
    printMtx ( _localStiffness, 6, 8 );
  double _yieldStress;  // Stress at which material fails
}
