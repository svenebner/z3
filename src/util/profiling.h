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

#include <fstream>
#include "util/vector.h"
#include "util/statistics.h"
#include <chrono>

struct node_runtime {
    double time;
    double mam_time;
    double quant_propagation_time;
    double qi_queue_time;
    double theory_time;
    unsigned node;
    bool entered_mam_loop;
};


/*
 * Nanosecond stopwatch similar to util/stopwatch.h
 */
class nanostopwatch {
    using clock_t = std::chrono::steady_clock;
    using time_point_t = clock_t::time_point;
    using duration_t = std::chrono::nanoseconds;

    time_point_t m_start;
    duration_t m_elapsed{0};
    duration_t m_last_update{0};
    bool m_running = false;

    static time_point_t now() { return clock_t::now(); }

public:
    void reset() {
        m_elapsed = duration_t::zero();
        m_last_update = duration_t::zero();
    }

    void start() {
        if (!m_running) {
            m_start = now();
            m_running = true;
        }
    }

    void stop() {
        if (m_running) {
            auto change = std::chrono::duration_cast<duration_t>(now() - m_start);
            m_elapsed += change;
            m_last_update += change;
            m_running = false;
        }
    }

    double get_nanoseconds() const {
        if (m_running) {
            const_cast<nanostopwatch*>(this)->stop();
            const_cast<nanostopwatch*>(this)->start();
        }
        return m_elapsed.count();
    }

    double get_seconds() const { return get_nanoseconds() / 1e9; }

    /*
     * Functionality to use same stop watch for total time and last node time
     */
    double get_last_update_nanoseconds() const {
        if (m_running) {
            const_cast<nanostopwatch*>(this)->stop();
            const_cast<nanostopwatch*>(this)->start();
        }
        return m_last_update.count();
    }

    double get_last_update_seconds() const { return get_last_update_nanoseconds() / 1e9; }

    void reset_last_update() {
        m_last_update = duration_t::zero();
    }
};

struct scoped_nanowatch {
    nanostopwatch& m_sw;

    scoped_nanowatch(nanostopwatch& sw, bool reset = false) : m_sw(sw) {
        if (reset) m_sw.reset();
        m_sw.start();
    }

    ~scoped_nanowatch() {
        m_sw.stop();
    }
};

/**
 * @brief Collects data used to help Viper related issues
 */
class profiling {
public:

protected:
    // All backtracking node numbers
    unsigned_vector backtracking_nodes;
    // All high time node numbers
    vector<node_runtime> node_runtime_vec;
    //Threshold what counts as high runtime
    const double high_time_threshold = 0.005;
    // Node in smtScope CDCL tree
    unsigned currentNode{0};
    // Nr. of loops iterations in mam.cpp state machine
    long unsigned mam_total_loop_itrs{0};
    bool entered_mam_loop{false};
    // Timing of last node
    nanostopwatch node_total_stopwatch;

    unsigned mam_high_time_count{0};
    // Nr. of states that took longer so dominate runtime
    unsigned high_time_count_total{0};

    svector<std::pair<long, unsigned>> backtrack_distances;

    std::vector<long unsigned> mam_case_counters = std::vector<long unsigned>(38);

    /*
     * File output
     */
    std::string file_output_dir;
    // Default file for general info
    std::ofstream* fs_general = nullptr;
    // Prepends the output directory
    [[nodiscard]] std::string concat_filepath(const std::string& filename) const {
        return file_output_dir + "/" + filename;
    }

    void write_data_to_files() const;

    /*
     * Helper Functions
     */
    void mam_loop_output(std::ofstream* out) const;
    double sum_mam_high_time_nodes() const;
    void high_time_backtracking_distance(const std::string& filename) const;
    void output_timing_csv(const std::string& filename) const;

    void add_backtracking_node(const unsigned node) { backtracking_nodes.push_back(node); }
    void add_node_runtime(const node_runtime& nRT) { node_runtime_vec.push_back(nRT); }

public:
    explicit profiling();
    ~profiling();
    // Update and output info for new pushed scope
    void scope_update();
    // Trace the backtracking steps
    void backtracking_update(unsigned num_scopes, unsigned new_lvl);

    void collect_statistics(statistics& st) const;

    // Stopwatches to track time spent in initialisation, core propagation and theories
    nanostopwatch total_propagation_stopwatch;
    nanostopwatch quant_propagation_stopwatch;
    nanostopwatch qi_queue_instantiation_stopwatch;
    nanostopwatch theories_stopwatch;
    nanostopwatch mam_total_stopwatch;

    /*
     * Methods used to profile src/smt/mam.cpp
     */
    void setup_mam() { entered_mam_loop = true; }
    void mam_loop_update() { mam_total_loop_itrs++; };
    void set_mam_loop_counters(const int enumCase) { mam_case_counters[enumCase]++; }
};
