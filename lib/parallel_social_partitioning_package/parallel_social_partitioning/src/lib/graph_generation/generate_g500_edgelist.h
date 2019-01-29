/******************************************************************************
 * generate_g500_edgelist.h 
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

#ifndef GENERATE_G500_EDGELIST_X69VQMCG
#define GENERATE_G500_EDGELIST_X69VQMCG

#include <vector>
#include "definitions.h"
#include "partition_config.h"

class generate_g500_edgelist {
public:
        generate_g500_edgelist();
        virtual ~generate_g500_edgelist();

        void generate_kronecker_edgelist( PartitionConfig & config, 
                                          std::vector< source_target_pair > & edge_list );
};


#endif /* end of include guard: GENERATE_G500_EDGELIST_X69VQMCG */
