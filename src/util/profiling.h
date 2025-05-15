/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    profiling.h

Abstract:

    Profiling support for viper toolchain.

Author:

    Sven Ebner (svenebner) 2025-04-22

Revision History:

--*/
#pragma once

#include<ostream>
#include "util/stopwatch.h"
#include "util/vector.h"
#include "util/statistics.h"



class profiling {
public:

protected:
    // Vector for each layer to get total time until backtrack
    vector<stopwatch> backtracking_vector;
    // All backtracking node numbers
    unsigned_vector backtracking_nodes;
    // All high time node numbers
    vector<std::tuple<unsigned, double>> high_time_nodes_total;
    // High time nodes in the mam loop
    vector<std::tuple<unsigned, double>> mam_high_time_nodes;
    //Threshold what counts as high runtime
    const double high_time_threshold = 0.005;
    // Node in smtScope CDCL tree
    unsigned currentNode{0};
    // Nr. of loops iterations in mam.cpp state machine
    long unsigned mam_total_loop_itrs{0};
    bool entered_mam_loop{false};
    // Timing of last mam.cpp state machine duration
    stopwatch mam_stopwatch;
    // Timing of last node
    stopwatch node_total_stopwatch;
    // Nr. of states that took longer so dominate runtime
    unsigned mam_high_time_count{0};

    unsigned high_time_count_total{0};

    svector<std::pair<long, unsigned>> backtrack_distances;




    std::ofstream* file_stream = nullptr;

    std::vector<long unsigned> mam_case_counters = std::vector<long unsigned>(38);
    //std::vector<stopwatch> mam_case_stopwatches = std::vector<stopwatch>(38);;

    void mam_loop_output(std::ofstream* out) const;
    double sum_mam_high_time_nodes() const;
    // Calc min distance for each high time node to backtracking step
    void high_time_backtracking_distance(std::ostream* out = nullptr) const;

    void add_backtracking_node(const unsigned node) { backtracking_nodes.push_back(node); }
    void add_mam_high_time_node(const unsigned node, const double time) { mam_high_time_nodes.push_back(std::tuple{node, time}); }
    void add_high_time_node_total(const unsigned node, const double time) {high_time_nodes_total.push_back(std::tuple{node, time}); }
public:
    profiling();
    ~profiling();
    // Update and output info for new pushed scope
    void scope_update(unsigned scope);
    // Trace the backtracking steps
    void backtracking_update(unsigned num_scopes, unsigned new_lvl);

    void collect_statistics(statistics& st) const;

    /*
     * Methods used to profile src/smt/mam.cpp
     */
    void setup_mam();
    void mam_loop_update();
    void exit_mam();
    void set_mam_loop_counters(const int enumCase) {mam_case_counters[enumCase]++;}
};
