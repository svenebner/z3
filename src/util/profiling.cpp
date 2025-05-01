/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    profiling.cpp

Abstract:

    Profiling support for viper toolchain.

Author:

    Sven Ebner (svenebner) 2025-04-22

Revision History:

--*/
#include "util/profiling.h"
#include "util/stopwatch.h"
#include "util/trace.h"
#include<iostream>


/**
 * @brief Updates the profiling state when entering a new scope.
 *
 * @param scope The scope level to update.
 */
void profiling::scope_update(const unsigned scope){

    backtracking_vector.setx(scope, stopwatch(), stopwatch());
    backtracking_vector[scope].start();
    const double currSeconds = mam_stopwatch.get_seconds();
    //Trace steps made before pushing new scope
    if ( entered_mam_loop && currSeconds >= 0.001) {
        high_time_count++;
        add_high_time_node(currentNode, currSeconds);
        TRACE("profiling_cdcl",
          tout << "node: " << currentNode << ", props in node: " << mam_loop_itrs <<
          ", entered loop: " << entered_mam_loop << ", time: " << currSeconds << "\n";);

    }

    currentNode++;
    mam_loop_itrs = 0;
    entered_mam_loop = false;
    mam_stopwatch.reset();

}

/**
 * @brief Updates the profiling state during backtracking.
 *
 * @param num_scopes The number of scopes being backtracked.
 * @param new_lvl The new scope level after backtracking.
 */
void profiling::backtracking_update(const unsigned num_scopes, const unsigned new_lvl) {

    TRACE("profiling_cdcl",
                  tout << "backtracking: " << num_scopes << ", new_lvl: " << new_lvl <<
                  ", node: " << currentNode << ", time since last here: " <<
                  backtracking_vector[new_lvl].get_seconds() << "\n";);
    add_backtracking_node(currentNode);
    currentNode++;

}

/**
 * @brief Collects profiling statistics and updates the provided statistics object.
 *
 * @param st Reference to the statistics object to update.
 */
void profiling::collect_statistics(statistics & st) const {

    st.update("high time count", high_time_count);
    st.update("max node", currentNode);
    std::ofstream file("timing.txt");
    high_time_backtracking_distance(&file);
    file.close();
}

/**
 * @brief Calculates and outputs the distance between high-time nodes and backtracking nodes.
 *
 * @param out Optional output stream to write the results. Defaults to std::cerr if null.
 */
void profiling::high_time_backtracking_distance(std::ostream * out) const {
    if (backtracking_nodes.empty()) return;

    std::ostream & os = out ? *out : std::cerr;

    os << "backtracking_nodes: " << backtracking_nodes.size() << ", high_time_nodes: " << high_time_nodes.size() << "\n";

    unsigned prev_backtracking_node = backtracking_nodes[0];

    // Edge case
    if (backtracking_nodes.size() == 1) {
        for (const std::tuple high_time_tuple : high_time_nodes) {
            const unsigned high_time_node = std::get<0>(high_time_tuple);
            const double time = std::get<1>(high_time_tuple);
            os << "node: " << high_time_node << ", backtrack dist: " << (high_time_node - prev_backtracking_node) << ", time: " << time << "\n";
        }
    }

    // Next backtracking node to fetch
    unsigned back_index = 2;
    unsigned curr_backtracking_node = backtracking_nodes[1];
    const unsigned vec_size = backtracking_nodes.size();
    for (const std::tuple high_time_tuple : high_time_nodes) {

        const unsigned high_time_node = std::get<0>(high_time_tuple);
        const double time = std::get<1>(high_time_tuple);

        while ( curr_backtracking_node < high_time_node && back_index < vec_size) {
            prev_backtracking_node = curr_backtracking_node;
            curr_backtracking_node = backtracking_nodes[back_index];
            back_index++;
        }

        const long back_dist = static_cast<long>(high_time_node) - prev_backtracking_node;
        const long front_dist = static_cast<long>(high_time_node) - curr_backtracking_node;

        const long min_dist = std::abs(back_dist) < std::abs(front_dist) ? back_dist : front_dist;
        os << "node: " << high_time_node << ", backtrack dist: " << min_dist << ", time: " << time << "\n";
    }
}

void profiling::setup_mam() {
    entered_mam_loop = true;
    mam_stopwatch.start();
}

void profiling::mam_loop_update() {
    mam_loop_itrs++;
}

void profiling::exit_mam() {
    mam_stopwatch.stop();
}