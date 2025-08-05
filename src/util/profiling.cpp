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
#include <filesystem>
#include "util/profiling.h"
#include<iostream>

/**
 * @brief Initializes profiling output directory and opens the general output file.
 */
profiling::profiling() {
    node_total_stopwatch.start();

    namespace fs = std::filesystem;
    std::ostringstream oss;
    const auto t = std::time(nullptr);
    oss << std::put_time(std::localtime(&t), "%Y-%m-%dT%H-%M-%S");
    file_output_dir = "profiling_outputs/" + oss.str();

    if (!fs::exists(file_output_dir)) {
        fs::create_directories(file_output_dir);
    }
}

/**
 * @brief Write data to file and close file stream at the end of a run
 */
profiling::~profiling() {
    // Get data of last scope, as it was never popped
    scope_update();
    write_data_to_files();
}

/**
 * @brief Updates the profiling state when entering a new scope, be that from push or backtracking.
 */
void profiling::scope_update() {
    node_total_stopwatch.stop();

    const double currSeconds = node_total_stopwatch.get_last_update_seconds();
    const double currMamSeconds = entered_mam_loop ? mam_total_stopwatch.get_last_update_seconds() : 0.0;

    const double currEMatchingSec = ematching_stopwatch.get_last_update_seconds();
    const double currQiQueueSec = qi_queue_instantiation_stopwatch.get_last_update_seconds();
    const double currTheorySec = theories_stopwatch.get_last_update_seconds();

    //Save runtime per node
    add_node_runtime({
        currSeconds, currMamSeconds, currEMatchingSec, currQiQueueSec, currTheorySec, currentNode, entered_mam_loop
    });

    currentNode++;
    entered_mam_loop = false;

    node_total_stopwatch.reset_last_update();
    mam_total_stopwatch.reset_last_update();
    ematching_stopwatch.reset_last_update();
    qi_queue_instantiation_stopwatch.reset_last_update();
    theories_stopwatch.reset_last_update();

    node_total_stopwatch.start();
}

/**
 * @brief Updates the profiling state during backtracking.
 *
 * @param num_scopes The number of scopes being backtracked.
 * @param new_lvl The new scope level after backtracking.
 */
void profiling::backtracking_update(const unsigned num_scopes, const unsigned new_lvl) {
    add_backtracking_node(currentNode);
    scope_update();
}

/**
 * @brief Writes all collected data to files.
 */
void profiling::write_data_to_files() const {
    output_general_timings("general_timings.csv");
    output_backtracking_nodes("backtracking.csv");
    output_timing_csv("timing.csv");
}

/**
 * @brief Collects profiling statistics and updates the provided statistics object.
 *
 * @param st Reference to the statistics object to update.
 */
void profiling::collect_statistics(statistics& st) const {
    st.update("PROFILE max node", currentNode);
    st.update("PROFILE time total propagation", total_propagation_stopwatch.get_seconds());
    st.update("PROFILE time e-matching", ematching_stopwatch.get_seconds());
    st.update("PROFILE time total mam", mam_total_stopwatch.get_seconds());
    st.update("PROFILE time quantifier queue instantiation", qi_queue_instantiation_stopwatch.get_seconds());
    st.update("PROFILE time theories propagation", theories_stopwatch.get_seconds());
    st.update("PROFILE time conflicts", total_conflict_stopwatch.get_seconds());
}

/**
 * @brief Writes all overall timers to file
 */
void profiling::output_general_timings(const std::string& filename) const {
    std::ofstream os_back(concat_filepath(filename));

    os_back <<
        "total_runtime,total_conflict_resolution,total_propagation,"
        "e-matching_time,total_mam_time,quantifier_queue_instantiation,theories_propagation"
        << "\n";
    os_back << node_total_stopwatch.get_seconds() << "," << total_conflict_stopwatch.get_seconds()
        << "," << total_propagation_stopwatch.get_seconds() << "," << ematching_stopwatch.get_seconds()
        << "," << mam_total_stopwatch.get_seconds() << "," << qi_queue_instantiation_stopwatch.get_seconds()
        << "," << theories_stopwatch.get_seconds() << "\n";
}

/**
 * @brief Writes all backtracking nodes to file
 */
void profiling::output_backtracking_nodes(const std::string& filename) const {
    std::ofstream os_back(concat_filepath(filename));

    os_back << "backtracking_node\n";

    for (const unsigned node : backtracking_nodes) {
        os_back << node << "\n";
    }
}

void profiling::output_timing_csv(const std::string& filename) const {
    std::ofstream os(concat_filepath(filename));

    os << "node,total_time,entered_mam_loop,mam_time,e_matching_time,qi_queue_time,theory_time\n";
    for (const auto& [total_time, mam_time, ematching_time,qi_queue_time, theory_time, node, entered_mam_loop] :
         node_runtime_vec) {
        os << node << "," << total_time << "," << entered_mam_loop << "," << mam_time << "," << ematching_time << ","
            << qi_queue_time << "," << theory_time << "\n";
    }
}
