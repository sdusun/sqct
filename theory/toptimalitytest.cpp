//     Copyright (c) 2012 Vadym Kliuchnikov sqct(dot)software(at)gmail(dot)com, Dmitri Maslov, Michele Mosca
//
//     This file is part of SQCT.
// 
//     SQCT is free software: you can redistribute it and/or modify
//     it under the terms of the GNU Lesser General Public License as published by
//     the Free Software Foundation, either version 3 of the License, or
//     (at your option) any later version.
// 
//     SQCT is distributed in the hope that it will be useful,
//     but WITHOUT ANY WARRANTY; without even the implied warranty of
//     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     GNU Lesser General Public License for more details.
// 
//     You should have received a copy of the GNU Lesser General Public License
//     along with SQCT.  If not, see <http://www.gnu.org/licenses/>.
// 

#include "toptimalitytest.h"
#include "output.h"
#include "es/exactdecomposer.h"
#include "numbers-stat.h"

#include <iostream>
#include <iomanip>
using namespace std;


static void print_list( std::vector<int>& a, std::vector<int>& b, std::vector<int>& c)
{
    cout << "sde(|.|^2)   total   min-T   max-T" << endl;
    cout << "                     gates   gates" << endl;

    for( int i = 0; i < a.size(); ++i )
        if( a[i] != 0 )
            cout <<  setw (10) << i << ":" <<
                     setw (7) << a[i] << " " <<
                     setw (7) << b[i] << " " <<
                     setw (7) << c[i] << endl;
}

toptimalitytest::toptimalitytest()
{
    init();
}

//assumes that initial node is identity matrix
static void getCircuit( circuit& res, const optNode* node, const vector<int>& genIdToGateId )
{
    const optNode* current = node;
    while( current->parent != 0 )
    {
        res.push_front( genIdToGateId[current->gen_id] );
        current = current->parent;
    }
}

void toptimalitytest::init()
{
    static const gateLibrary& gl = gateLibrary::instance();
    const bool verbose  = true;
    typedef matrix2x2<int> m;

    ogc.m_generators = { m::X(), m::Y(), m::Z(), m::H(), m::P(), m::P().conjugateTranspose() };
    vector<int> genIdToGateId = { gl.X, gl.Y, gl.Z, gl.H, gl.P, gl.Pd };
    ogc.m_cost = { 1,2,1,10,40,40 };
    ogc.m_initial = { m::Id() };
    vector<int> initialGates = { gl.Id };
    ogc.m_initial_cost = {0};
    ogc.generate();

    //produce couning bounds
    typedef columnsCounter<8> cC;
    unique_ptr<cC> cp( new cC );
    cp->generate_all_numbers();
    cp->count_all_columns();

    //setup bounds
    for( int i = 0 ; i < cp->sde_stat.size() ; ++i )
        og.m_sde_required.at(i) = cp->sde_stat[i];

    m_clifford.resize( ogc.unique_elements().size() );
    vector<int> pauliCounts( ogc.unique_elements().size() );

    auto k = m_clifford.begin();
    auto k1 = pauliCounts.begin(); //number of Pauli gates used in each generator

    if( verbose ) cout << "Single qubit Clifford circuits:" << endl;

    for( auto i : ogc.unique_elements() )
    {
        getCircuit(*k,i.get(),genIdToGateId);
        auto counts = k->count();
        *k1 = counts[ gl.Z ] + counts[ gl.X ] + counts[ gl.Y ];

        if( verbose ) { k->toStreamSym(cout); cout << " " << *k1 << "," << endl; }

        og.m_generators.push_back( i->unitary * m::T() );
        og.m_cost.push_back( 1 );
        og.m_initial.push_back( i->unitary );
        ++k;++k1;
    }

    og.m_initial_cost.resize( og.m_initial.size(), 0 );
    og.generate();
    if( verbose ) print_list( og.m_sde_found, og.m_min_cost, og.m_max_cost );

    ///////////////////////////////////////////////////////////////////////////

    cout << "Maximal number of Paulies in prefix:" << endl;
    int maxPauliCounts = 0;
    for( auto i : og.unique_elements() )
    {
        if(  i->unitary.max_sde_abs2() <= 3  )
        {
            const optNode* current = i.get();
            int paulis = pauliCounts[current->gen_id];
            while( current->parent != 0 )
            {
                current = current->parent;
                paulis += pauliCounts[current->gen_id];
            }
            maxPauliCounts = std::max( maxPauliCounts, paulis);
        }
    }
    cout << maxPauliCounts << endl;

    ///////////////////////////////////////////////////////////////////////////

    exactDecomposer ed;
    int counter = 0;
    int nonoptimal = 0;
    double den = 1.0 / (double) og.unique_elements().size();
    cout << "Percent of checked gates:" << endl;
    for( auto i : og.unique_elements() )
    {
        circuit c;
        ed.decompose(i->unitary,c);
        auto counts = gl.toCliffordT(c.count());
        int Tc = counts[gl.T] + counts[ gl.Td ];
        if( i->cost != Tc )
        {
            cout << "*";
            nonoptimal++;
        }

        counter++;
        if( counter % 100000 == 0 )
            cout << (double) counter * den << endl;
    }
    cerr << "non optimal:" << nonoptimal << endl;
}

