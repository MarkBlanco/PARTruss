#include <vector>
#include <iostream>
#include <json.hpp>
#ifndef IDX2C
// Macro to map from 
#define IDX2C(i,j,ld) (((j)*(ld))+(i))

// NOTE: Only implementing stiffness method for 2D (for now?)!


class Truss
{
public:
    Truss(std::vector<Element> & Elements, std::vector<Node> Nodes);
    std::vector<double> solve();    // This is where the magic needs to happen...
    std::ostream & outputJson();    // Output the solved json version to be visualized
    
// Getters:
    double const getWeight() const { return this->_weight; }
//private:
    // Representation:
    std::vector<Element> _elements;
    std::std::vector<Node> _nodes;
    double _totalWeight = 0.0;
    
    
    // Utility:
    bool addNode( Node & n );
    bool addElement( Element & e );
};