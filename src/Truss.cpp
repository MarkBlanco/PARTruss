// Truss class implementation
// Note: IDX2C(i,j,ld) (((j)*(ld))+(i)) defined as shortcut for indexing into matrix

#include <cstddef>
#include <valarray>
#include <cmath>
#include "Truss.hpp"
#include "json.hpp"

using json = nlohmann::json;

Truss::Truss(std::vector<Element> & Elements, std::vector<Node> & Nodes)
{
    // Iterate over elements and nodes and add them:
    for (int i = 0; i < Nodes.size(); i++)
    {
        if (this->addNode(Nodes[i]))
        {
            Nodes[i].setId(i);
        }
    }
    
    for (int i = 0; i < Elements.size(); i++)
    {
        if (this->addElement(Elements[i]))
        {
            Elements[i].setId(i);
            // _totalWeight += Elements[i].getWeight();
        }
    }
}

 // This is where the magic needs to happen...
 // Very much based off of the approach in 
 // https://www.mathworks.com/matlabcentral/fileexchange/31350-truss-solver-and-genetic-algorithm-optimzer?focused=5188720&tab=function#feedbacks
std::vector<double *> Truss::solve(std::valarray<double> & Forces, 
    std::valarray<double> & Displacements )   
{
    int numNodes = this->_nodes.size();
    int numEdges = this->_elements.size();
    // Allocate a bunch of arrays:
    double* K = (double*)calloc( sizeof(double), pow(numNodes * 3, 2) ); // Global stiffness matrix
    bool* Re = (bool*)calloc( sizeof(bool), numNodes * 3 );          // All restraints on each node 
    double* Ld = (double*)calloc( sizeof(double), numNodes * 3 );        // All loads on each node
    if ( K == NULL || Re == NULL || Ld == NULL )
    {
        std::cerr << "ERROR: Malloc failed to allocate memory in the truss solver member function!\n";
        exit(EXIT_FAILURE);
    }
    // Populate the load and restraint matrices from the nodes:
    for ( int i = 0 ; i < this->_nodes.size(); i ++ )
    {
        int nodeId = this->_nodes[i].getId();   // Note that node numbering starts at 0, not 1 
        Re[IDX2C(nodeId, 0, numNodes)] = this->_nodes[i].getConstX();
        Re[IDX2C(nodeId, 1, numNodes)] = this->_nodes[i].getConstY();
        Re[IDX2C(nodeId, 2, numNodes)] = this->_nodes[i].getConstZ();
        Ld[IDX2C(nodeId, 0, numNodes)] = this->_nodes[i].getLoadX();
        Ld[IDX2C(nodeId, 1, numNodes)] = this->_nodes[i].getLoadY();
        Ld[IDX2C(nodeId, 2, numNodes)] = this->_nodes[i].getLoadZ();
    }
    // Now for each element, add its global-coordinate stiffness matrix to the system matrix K
    // The locations where each quadrant of the element matrix fit into the system matrix
    //are based on the indices of the nodes at each end of the element.
    for (int i = 0; i < this->_elements.size(); i++)
    {             
        int node1 = this->_elements[i].getStart()->getId();
        int node2 = this->_elements[i].getEnd()->getId();
                        // Indices based on first node...and second node
        int indices[6] = { 3*node1, 3*node1+1, 3*node1+2, 3*node2, 3*node2+1, 3*node2+2 };
        const double * local_stiffness = this->_elements[i].getLocalStiffness();
        // NOTE: Node numbering starts at 0; hence why the indexing above works.
        for (int j = 0; j < 6; j ++ )
        {
            for (int k = 0; k < 6; k++ )
            {
                // Add the values from the element's stiffness matrix onto the global matrix
                K[IDX2C(indices[j], indices[k], numNodes*3)] += local_stiffness[IDX2C(j, k, 6)];
            }
        }
    }
    // Now that the global stiffness matrix exists, save it!
    this->_systemStiffnessMatrix = K;
    this->_stiffnessMatrixSize = numNodes * 3;
    // Now need to filter the stiffness matrix based on which nodes are/aren't restrained:
    // (it's a degrees of freedom indexing vector)
    std::vector<int> dog;
    for (int i = 0; i < numNodes*3; i++)
    {   
        // Iterate over Re as a flattened matrix (vector where each group of three rows
        // is the x,y,z restrictions on movement of that node)
        if (!Re[i]) // If that direction is a unrestrained degree of free motion for the node, save the
                    // value. These indices will be filters on which the system stiffness matrix
                    // and load vector are sliced.
        {
            dog.push_back(i);
        }
    }
    // TODO: use thrust to efficiently filter the K matrix and Ld vector (view Ld as a flattened matrix)?
    // Entire row-columns that correspond to the axis on which a node is constrained must be dropped.
    // Corresponding forces in restrained directions for each node in the external force matrix/vector
    //are also to be dropped.
    // Construct the simplified system stiffness matrix (missing entries corresponding to indices in 
    //degrees_of_freedom matrix)
    double * A = (double*)calloc( sizeof(double), pow(dog.size(), 2) );
    // Vector to hold the know external forces:
    double * f = (double*)calloc( sizeof(double), dog.size() );
    if ( A == NULL )
    {
        std::cerr << "ERROR: Malloc failed to allocate memory in the truss solver member function!\n";
        exit(EXIT_FAILURE);
    }
    // Copying over just the values that matter into A and f
    for (int i = 0; i < dog.size(); i++) {
        for (int k = 0; k < dog.size(); k++) {
            A[IDX2C(i, k, dog.size())] = K[IDX2C(dog[i], dog[k], 3*numNodes)];
        }
        f[i] = Ld[dog[i]];  // This turns the 3 x numNodes matrix Ld into a filtered column vector
    }
    // At this point the system can now be solved for the displacement of each node!
    // Formula is d = A\f in MATLAB, or d = A^-1 f in more mathy terms.
    // TODO: implement solving the matrix system using CUDA or something. Should that be done in here
    // or should A and f be stored/returned and then the solving can be done outside, perhaps
    // in a few different ways?

    // Note that indices not stored in dog have a 0 displacement for that node and coordinate direction (xyz).
    // Then the force in each element is k( (1/L)(dx, dy, dz)dot(displacement_node_2 - displacement_node1) )
    // Where displacement of each node is a 3-vector for the x,y,z components.
    // At this point the changes in each location should be written back, the forces in each element should be stored,
    // And json output should be written out.
    std::vector<double *> systemEqns = { A, f };
    return systemEqns;
}

void Truss::outputJSON(std::ostream & f)
{
    // This needs to output JSON stuff...
    // Go through each node and output it in json formatted goodness
    // Go through each vertex and output it in json format
    int numNodes = this->_nodes.size();
    int numEdges = this->_elements.size();
    json j;
    std::string vertices = "[";
    for (int i = 0; i < numNodes; i++)
    {
        vertices += "{\"XYZPosition\": " +  array2string(this->_nodes[i].getCoords());
        vertices += ", \"XYZAppliedForces\": " + array2string(this->_nodes[i].getLoads());
        vertices += ", \"Anchored\": " + array2string(this->_nodes[i].getConstraints() ) + "}";
        if ( i < numNodes - 1) { vertices += ", "; }
    }
    j["Vertices"] = json::parse(vertices);
    
    std::string edges = "[";
    for (int i = 0; i < numEdges; i++)
    {
        std::array<int,2> endpoints = {this->_elements[i].getStart()->getId(), this->_elements[i].getEnd()->getId()};
        edges += "{\"Endpoints\": " + array2string(endpoints);
    	edges += ", \"ElasticModulus\": " +  std::to_string(this->_elements[i].getMod());
    	edges += ", \"SectionArea\": " + std::to_string(this->_elements[i].getArea());  /*,
    	I think we need to compute these still..
    	edges += ", \"Force\": ", array2string(this->_elements[i]->???);,
    	edges += ", \"Stress\": " + this->_elements[i]->???;*/
        if ( i < numEdges - 1) { edges += ", "; }
    }
    j["Edges"] = json::parse(edges);
    
    f << j;
}


bool Truss::addNode( Node & n )
{
    bool alreadyExists = false;
    for ( int i = 0; i < this->_nodes.size(); i++ )
    {
        alreadyExists |= nodeEqual(n, this->_nodes[i] );
    }
    if ( alreadyExists ){
        std::cerr << "Error: this node has already been added to the truss. Skipping!\n";
        return false;
    }
    _nodes.push_back(n);
    return true;
}

bool Truss::addElement( Element & e )
{
    bool alreadyExists = false;
    for ( int i = 0; i < this->_elements.size(); i++ )
    {
        alreadyExists |= elemEqual(this->_elements[i], e);
    }
    if ( alreadyExists ){
        std::cerr << "Error: this element has already been added to the truss. Skipping!\n";
        return false;
    }
    _elements.push_back(e);
    return true;   
}

