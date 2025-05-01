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
    vector<std::tuple<unsigned, double>> high_time_nodes;
    // Node in smtScope CDCL tree
    unsigned currentNode{0};
    // Nr. of loops iterations in mam.cpp state machine
    unsigned mam_loop_itrs{0};
    bool entered_mam_loop{false};
    // Timing of last mam.cpp state machine duration
    stopwatch mam_stopwatch;
    // Nr. of states that took longer so dominate runtime
    unsigned high_time_count{0};

public:
    explicit profiling() = default;

    // Update and output info for new pushed scope
    void scope_update(unsigned scope);
    // Trace the backtracking steps
    void backtracking_update(unsigned num_scopes, unsigned new_lvl);

    void collect_statistics(statistics& st) const;
    // Calc min distance for each high time node to backtracking step
    void high_time_backtracking_distance(std::ostream* out = nullptr) const;
    void setup_mam();
    void mam_loop_update();
    void exit_mam();

    void add_backtracking_node(const unsigned node) { backtracking_nodes.push_back(node); }
    void add_high_time_node(const unsigned node, const double time) { high_time_nodes.push_back(std::tuple{node, time}); }

    // -----------------------------------
    //
    // Accessors
    //
    // -----------------------------------

    unsigned& get_mam_loop_itrs() { return mam_loop_itrs; }
    bool& get_entered_mam_loop() { return entered_mam_loop; }
    stopwatch& get_mam_stopwatch() { return mam_stopwatch; }
};
