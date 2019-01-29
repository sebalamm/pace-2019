/******************************************************************************
 * parallel_label_compress.cpp 
 *
 * Source of the Parallel Partitioning Program
 ******************************************************************************
 * Copyright (C) 2014 Christian Schulz <christian.schulz@kit.edu>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 *****************************************************************************/

#include <argtable2.h>
#include <iostream>
#include <math.h>
#include <iomanip>
#include <mpi.h>
#include <regex.h>
#include <sstream>
#include <stdio.h>
#include <string.h> 

#include "communication/mpi_tools.h"
#include "communication/dummy_operations.h"
#include "data_structure/parallel_graph_access.h"
#include "distributed_partitioning/distributed_partitioner.h"
#include "graph_generation/generate_kronecker.h"
#include "io/parallel_graph_io.h"
#include "io/parallel_vector_io.h"
#include "macros_assertions.h"
#include "parse_parameters.h"
#include "partition_config.h"
#include "random_functions.h"
#include "timer.h"
#include "tools/distributed_quality_metrics.h"
#include "graph_generation/generate_rgg.h"

int main(int argn, char **argv) {

        MPI::Init(argn, argv);    /* starts MPI */

        PartitionConfig partition_config;
        std::string graph_filename;

        int ret_code = parse_parameters(argn, argv, 
                                        partition_config, 
                                        graph_filename); 

        if(ret_code) {
                MPI::Finalize();
                return 0;
        }

        PEID rank = MPI::COMM_WORLD.Get_rank();
        PEID size = MPI::COMM_WORLD.Get_size();

        if(rank == ROOT) {
                std::cout <<  "log> cluster coarsening factor is set to " <<  partition_config.cluster_coarsening_factor  << std::endl;
        }

        partition_config.stop_factor /= partition_config.k;
        if(rank != 0) partition_config.seed = partition_config.seed*size+rank; 

        srand(partition_config.seed);

        parallel_graph_access G;
        timer t;
        if(partition_config.generate_kronecker) {
                if( rank == ROOT ) std::cout <<  "generating a kronecker graph"  << std::endl;
                t.restart();

                generate_kronecker gk;
                gk.generate_kronecker_graph( partition_config, G);
                if( rank == ROOT ) std::cout <<  "generation of kronecker graph took " <<  t.elapsed()  << std::endl;
        } else if(partition_config.generate_rgg) {
                if( rank == ROOT ) std::cout <<  "generating a rgg graph"  << std::endl;
                t.restart();

                generator_rgg grgg;
                PartitionConfig copy = partition_config;
                copy.seed = 1;
                grgg.generate(partition_config, G);
                if( rank == ROOT ) std::cout <<  "generation of rgg graph took " <<  t.elapsed()  << std::endl;

        } else {
                parallel_graph_io::readGraphWeighted(G, graph_filename, rank, size);
                if( rank == ROOT ) std::cout <<  "took " <<  t.elapsed()  << std::endl;
        }

        random_functions::setSeed(partition_config.seed);
        parallel_graph_access::set_comm_rounds( partition_config.comm_rounds/size );
        parallel_graph_access::set_comm_rounds_up( partition_config.comm_rounds/size);
        distributed_partitioner::generate_random_choices( partition_config );

        G.printMemoryUsage(std::cout);

        std::cout <<  G.number_of_local_edges()  << std::endl;

        MPI::COMM_WORLD.Barrier();
        {
                t.restart();
                if( rank == ROOT ) std::cout <<  "running collective dummy operations ";
                dummy_operations dop;
                dop.run_collective_dummy_operations();
        }
        MPI::COMM_WORLD.Barrier();

        if( rank == ROOT ) {
                std::cout <<  "took " <<  t.elapsed()  << std::endl;
        }

        //compute some stats
        EdgeWeight interPEedges = 0;
        EdgeWeight localEdges = 0;
        forall_local_nodes(G, node) {
                forall_out_edges(G, e, node) {
                        NodeID target = G.getEdgeTarget(e);
                        if(!G.is_local_node(target)) {
                                interPEedges++;
                        } else {
                                localEdges++;
                        }
                } endfor
        } endfor

        EdgeWeight globalInterEdges = 0;
        EdgeWeight globalIntraEdges = 0;
        MPI::COMM_WORLD.Reduce( &interPEedges, &globalInterEdges, 1, MPI_UNSIGNED_LONG, MPI_SUM, ROOT);
        MPI::COMM_WORLD.Reduce( &localEdges,   &globalIntraEdges, 1, MPI_UNSIGNED_LONG, MPI_SUM, ROOT);

        if( rank == ROOT ) {
                std::cout <<  "log> ghost edges " <<  globalInterEdges/(double)G.number_of_global_edges() << std::endl;
                std::cout <<  "log> local edges " <<  globalIntraEdges/(double)G.number_of_global_edges() << std::endl;
        }

        t.restart();
	double epsilon = (partition_config.inbalance)/100.0;
        partition_config.number_of_overall_nodes = G.number_of_global_nodes();
        partition_config.upper_bound_partition   = (1+epsilon)*ceil(G.number_of_global_nodes()/(double)partition_config.k);

        distributed_partitioner dpart;
        dpart.perform_partitioning( partition_config, G);

        MPI::COMM_WORLD.Barrier();


        double running_time = t.elapsed();
        distributed_quality_metrics qm;
        EdgeWeight edge_cut = qm.edge_cut( G );
        double balance  = qm.balance( partition_config, G );

        if( rank == ROOT ) {
                std::cout << "log>" << "=====================================" << std::endl;
                std::cout << "log>" << "============AND WE R DONE============" << std::endl;
                std::cout << "log>" << "=====================================" << std::endl;
                std::cout <<  "log>total partitioning time elapsed " <<  running_time << std::endl;
                std::cout <<  "log>final edge cut " <<  edge_cut  << std::endl;
                std::cout <<  "log>final balance "  <<  balance   << std::endl;
        }


#ifndef NDEBUG
        MPI::Status st;
        while(MPI::COMM_WORLD.Iprobe(MPI::ANY_SOURCE,MPI::ANY_TAG,st)){
                std::cout <<  "attention: still incoming messages! rank " <<  rank <<  " from " <<  st.Get_source()  << std::endl;

                unsigned long message_length = st.Get_count(MPI_UNSIGNED_LONG);
                std::vector<NodeID> message; message.resize(message_length);
                MPI::COMM_WORLD.Recv( &message[0], message_length, MPI_UNSIGNED_LONG, st.Get_source(), st.Get_tag()); 
        } 
#endif

        parallel_vector_io pvio;
        std::string filename("tmppartition");
        pvio.writePartitionSimpleParallel<NodeID>(G, filename);
        MPI::Finalize();
}
