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

/**
 * @brief Initializes profiling output directory and opens the general output file.
 */
profiling::profiling() {
    namespace fs = std::filesystem;
    std::ostringstream oss;
    const auto t = std::time(nullptr);
    oss << std::put_time(std::localtime(&t), "%Y-%m-%dT%H-%M-%S");
    file_output_dir = "profiling_outputs/" + oss.str();

    if (!fs::exists(file_output_dir)) {
        fs::create_directories(file_output_dir);
    }
    fs_general = new std::ofstream(file_output_dir + "/profiling_output.txt");
}

/**
 * @brief Write data to file and close file stream at the end of a run
 */
profiling::~profiling() {
    write_data_to_files();
    if (fs_general) {
        fs_general->close();
        delete fs_general;
        fs_general = nullptr;
    }
}

/**
 * @brief Updates the profiling state when entering a new scope, be that from push or backtracking.
 */
void profiling::scope_update() {
    //backtracking_vector.setx(scope, stopwatch(), stopwatch());
    //backtracking_vector[scope].start();

    node_total_stopwatch.stop();

    const double currSeconds = node_total_stopwatch.get_seconds();
    const double currMamSeconds = mam_stopwatch.get_seconds();

    //Save runtime per node
    add_node_runtime({currSeconds, currMamSeconds, currentNode, entered_mam_loop});
    if (currSeconds > high_time_threshold) {
        high_time_count_total++;
        if (entered_mam_loop && currMamSeconds > high_time_threshold) {
            mam_high_time_count++;
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
    scope_update();
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
    std::ostream& os = out ? *out : std::cerr;

    os << "mam loop iterations: " << mam_total_loop_itrs << "\n";
    for (unsigned i = 0; i < mam_case_counters.size(); i++) {
        if (mam_total_loop_itrs > 0) {
            const double percent = (static_cast<double>(mam_case_counters[i]) / static_cast<double>(
                mam_total_loop_itrs)) * 100.0;
            if (percent > 1) {
                os << opcode_names[i] << ": " << mam_case_counters[i];
                os << " (" << percent << "%)" << "\n";
            }
        }
    }
    os << "\n";
};

/**
 * @brief Writes all collected data to files.
 */
void profiling::write_data_to_files() const {
    (*fs_general) << "timings:\n"
        << "quantifier propagation: " << quant_propagation_stopwatch.get_seconds() << "\n"
        << "    cumulative mam high time: " << sum_mam_high_time_nodes() << "\n"
        << "quantifier queue instantiation: " << instantiation_stopwatch.get_seconds() << "\n"
        << "theories propagation: " << theories_stopwatch.get_seconds() << "\n\n";
    mam_loop_output(fs_general);
    high_time_backtracking_distance("backtracking.csv");
    output_timing_csv("timing.csv");
}

/**
 * @brief Collects profiling statistics and updates the provided statistics object.
 *
 * @param st Reference to the statistics object to update.
 */
void profiling::collect_statistics(statistics& st) const {
    st.update("PROFILE mam high time count", mam_high_time_count);
    st.update("PROFILE high time count total", high_time_count_total);
    st.update("PROFILE max node", currentNode);
    st.update("PROFILE cumulative mam high time", sum_mam_high_time_nodes());
    st.update("PROFILE time quantifier propagation", quant_propagation_stopwatch.get_seconds());
    st.update("PROFILE time quantifier queue instantiation", instantiation_stopwatch.get_seconds());
    st.update("PROFILE time theories propagation", theories_stopwatch.get_seconds());
}

/**
 * @brief Sum of all mam_times that are over high_time_threshold
 */
double profiling::sum_mam_high_time_nodes() const {
    double sum = 0.0;
    for (const node_runtime& nRt : node_runtime_vec) {
        if (const double mam_time = nRt.mam_time; mam_time > high_time_threshold) {
            sum += mam_time;
        }
    }
    return sum;
}

/**
 * @brief Writes all backtracking nodes to file and adds info to the general file.
 */
void profiling::high_time_backtracking_distance(const std::string& filename) const {
    std::ofstream os_back(concat_filepath(filename));

    os_back << "backtracking_node\n";

    if (backtracking_nodes.empty()) return;

    for (const unsigned node : backtracking_nodes) {
        os_back << node << "\n";
    }

    // Output general info into separate file
    (*fs_general) << "backtracking_nodes: " << backtracking_nodes.size() << ", mam_high_time_nodes: " <<
        mam_high_time_count
        << ", high_time_nodes_total: " << high_time_count_total << ", threshold: " << high_time_threshold << "\n";
}

void profiling::output_timing_csv(const std::string& filename) const {
    std::ofstream os(concat_filepath(filename));

    os << "node,total_time,entered_mam_loop,mam_time\n";
    for (const auto& [total_time, mam_time, node, entered_mam_loop] : node_runtime_vec) {
        os << node << "," << total_time << "," << entered_mam_loop << "," << mam_time << "\n";
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
