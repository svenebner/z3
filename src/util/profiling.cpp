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

static const char* opcode_names[] = {
    "INIT1", "INIT2", "INIT3", "INIT4", "INIT5", "INIT6", "INITN",
    "BIND1", "BIND2", "BIND3", "BIND4", "BIND5", "BIND6", "BINDN",
    "YIELD1", "YIELD2", "YIELD3", "YIELD4", "YIELD5", "YIELD6", "YIELDN",
    "COMPARE", "CHECK", "FILTER", "CFILTER", "PFILTER", "CHOOSE", "NOOP", "CONTINUE",
    "GET_ENODE",
    "GET_CGR1", "GET_CGR2", "GET_CGR3", "GET_CGR4", "GET_CGR5", "GET_CGR6", "GET_CGRN",
    "IS_CGR"
};

profiling::profiling() {
    file_stream = new std::ofstream("profiling_output.txt");
}

profiling::~profiling() {
    if (file_stream) {
        file_stream->close();
        delete file_stream;
        file_stream = nullptr;
    }
}
/**
 * @brief Updates the profiling state when entering a new scope, be that from push or backtracking.
 *
 * @param scope The scope level to update.
 */
void profiling::scope_update(const unsigned scope){

    //backtracking_vector.setx(scope, stopwatch(), stopwatch());
    //backtracking_vector[scope].start();

    node_total_stopwatch.stop();

    const double currSeconds = node_total_stopwatch.get_seconds();
    const double currMamSeconds = mam_stopwatch.get_seconds();

    //Trace steps made before pushing new scope
    if (currSeconds >= high_time_threshold) {
        high_time_count_total++;
        add_high_time_node_total(currentNode, currSeconds);
        // Save if took significant time
        if ( entered_mam_loop && currMamSeconds >= high_time_threshold) {
            mam_high_time_count++;
            add_mam_high_time_node(currentNode, currMamSeconds);
        }
    }


    currentNode++;
    //mam_total_loop_itrs = 0;
    entered_mam_loop = false;
    mam_stopwatch.reset();
    node_total_stopwatch.reset();
    node_total_stopwatch.start();

}

/**
 * @brief Updates the profiling state during backtracking.
 *
 * @param num_scopes The number of scopes being backtracked.
 * @param new_lvl The new scope level after backtracking.
 */
void profiling::backtracking_update(const unsigned num_scopes, const unsigned new_lvl) {

    // TRACE("profiling_cdcl",
    //               tout << "backtracking: " << num_scopes << ", new_lvl: " << new_lvl <<
    //               ", node: " << currentNode << ", time since last here: " <<
    //               backtracking_vector[new_lvl].get_seconds() << "\n";);

    add_backtracking_node(currentNode);
    scope_update(new_lvl);

}
/**
 * @brief Outputs MAM loop profiling statistics.
 *
 * Prints the total MAM loop iterations and, for each opcode with >1% share,
 * its name, count, and percentage.
 *
 * @param out Optional output stream; defaults to std::cerr.
 */
void profiling::mam_loop_output(std::ofstream* out) const {
    std::ostream & os = out ? *out : std::cerr;

    try {
        os.imbue(std::locale("en_US.UTF-8"));
    } catch (const std::runtime_error&) {
        // Fallback to the user's default locale if en_US.UTF-8 is not available
        os.imbue(std::locale(""));
    }

    os << "mam loop iterations: " << mam_total_loop_itrs << "\n";
    for (unsigned i = 0; i < mam_case_counters.size(); i++) {

        if (mam_total_loop_itrs > 0) {
            const double percent = (static_cast<double>(mam_case_counters[i]) / static_cast<double>(mam_total_loop_itrs)) * 100.0;
            if (percent > 1) {
                os << opcode_names[i] << ": " << mam_case_counters[i];
                os << " (" << percent << "%)" << "\n";
            }
        }
    }
    os << "\n";
};

/**
 * @brief Collects profiling statistics and updates the provided statistics object.
 *
 * @param st Reference to the statistics object to update.
 */
void profiling::collect_statistics(statistics & st) const {

    st.update("mam high time count", mam_high_time_count);
    st.update("high time count total", high_time_count_total);
    st.update("max node", currentNode);
    st.update("cumulative mam high time", sum_mam_high_time_nodes());
    mam_loop_output(file_stream);
    high_time_backtracking_distance(file_stream);
}

double profiling::sum_mam_high_time_nodes() const {
    double sum = 0.0;
    for (const auto& tuple : mam_high_time_nodes) {
        sum += std::get<1>(tuple);
    }
    return sum;
}

/**
 * @brief Calculates and outputs the distance between high-time nodes and backtracking nodes.
 *
 * @param out Optional output stream to write the results. Defaults to std::cerr if null.
 */
void profiling::high_time_backtracking_distance(std::ostream * out) const {
    if (backtracking_nodes.empty()) return;

    std::ostream & os = out ? *out : std::cerr;

    auto min_dist_counts = std::unordered_map<long, unsigned>();

    os << "backtracking_nodes: " << backtracking_nodes.size() << ", mam_high_time_nodes: " << mam_high_time_nodes.size()
    << ", high_time_nodes_total: " << high_time_count_total<< ", threshold: " << high_time_threshold <<"\n";

    unsigned prev_backtracking_node = backtracking_nodes[0];

    // Edge case
    if (backtracking_nodes.size() == 1) {
        for (const std::tuple high_time_tuple : mam_high_time_nodes) {
            const unsigned high_time_node = std::get<0>(high_time_tuple);
            const double time = std::get<1>(high_time_tuple);
            os << "node: " << high_time_node << ", backtrack dist: " << (high_time_node - prev_backtracking_node) << ", time: " << time << "\n";
        }
    }

    // Next backtracking node to fetch
    unsigned back_index = 2;
    unsigned curr_backtracking_node = backtracking_nodes[1];
    const unsigned backtrack_vec_size = backtracking_nodes.size();

    // Iterate over mam_high_time_nodes at the same time, should be subset of high_time_nodes_total
    unsigned mam_index = 0;
    const unsigned mam_size = mam_high_time_nodes.size();
    unsigned mam_curr_node = 0;
    for (const std::tuple high_time_tuple : high_time_nodes_total) {

        const unsigned high_time_node = std::get<0>(high_time_tuple);
        const double total_time = std::get<1>(high_time_tuple);

        while (mam_index < mam_size && mam_curr_node < high_time_node) {
            mam_curr_node = std::get<0>(mam_high_time_nodes[mam_index]);
            mam_index++;

        }

        while ( curr_backtracking_node < high_time_node && back_index < backtrack_vec_size) {
            prev_backtracking_node = curr_backtracking_node;
            curr_backtracking_node = backtracking_nodes[back_index];
            back_index++;
        }

        const long back_dist = static_cast<long>(high_time_node) - prev_backtracking_node;
        const long front_dist = static_cast<long>(high_time_node) - curr_backtracking_node;

        const long min_dist = std::abs(back_dist) < std::abs(front_dist) ? back_dist : front_dist;
        os << "node: " << high_time_node << ", backtrack dist: " << min_dist << ", time: " << total_time;
        if (mam_curr_node == high_time_node) {
            double mam_time = std::get<1>(mam_high_time_nodes[mam_index-1]);
            os << ", mam time: " << mam_time << ", mam %: " << mam_time/total_time;

        }
        os << "\n";

        min_dist_counts[min_dist]++;
    }
    os << "min_dist counts:\n";
    std::vector<std::pair<long, unsigned>> sorted_min_dist_counts(min_dist_counts.begin(), min_dist_counts.end());
    std::sort(sorted_min_dist_counts.begin(), sorted_min_dist_counts.end(), [](const auto& a, const auto& b) {
        return a.second > b.second; // Sort by value in descending order
    });
    for (const auto& [fst, snd] : sorted_min_dist_counts) {
        os << "min_dist: " << fst << ", count: " << snd << "\n";
    }
}

void profiling::setup_mam() {
    entered_mam_loop = true;
    mam_stopwatch.start();
}

void profiling::mam_loop_update() {
    mam_total_loop_itrs++;
}

void profiling::exit_mam() {
    mam_stopwatch.stop();
}