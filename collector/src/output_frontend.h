/*
 * output_frontend.h -- main objects for the JSON/InfluxDB output frontends
 * Developer: Francesco Montorsi.
 * (C) Copyright 2018 Francesco Montorsi

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

//------------------------------------------------------------------------------
// Includes
//------------------------------------------------------------------------------

#include <array>
#include <set>
#include <string.h>
#include <string>
#include <vector>

// Prometheus
#include <prometheus/gauge.h>
#include <prometheus/labels.h>
#include <prometheus/family.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/detail/future_std.h>

#include <chrono>
#include <cstdlib>
#include <memory>
#include <thread>

//------------------------------------------------------------------------------
// Constants
//------------------------------------------------------------------------------

#define CMONITOR_MEASUREMENT_NAME_MAXLEN (64)
#define CMONITOR_MEASUREMENT_VALUE_MAXLEN (256) // some strings like e.g. "uname -a" can be pretty long

//------------------------------------------------------------------------------
// Forward declarations
//------------------------------------------------------------------------------

struct _influx_client_t;
typedef struct _influx_client_t influx_client_t;

//------------------------------------------------------------------------------
// The JSON/InfluxDB frontend
//
// Allows only the following logical hierarchy:
//  SAMPLE
//   -> SECTION
//      -> MEASUREMENT
// or
//  SAMPLE
//   -> SECTION
//      -> SUBSECTION
//         -> MEASUREMENT
//------------------------------------------------------------------------------

class CMonitorOutputFrontend {
public:
    CMonitorOutputFrontend(const std::string& json_file_prefix = "")
    {
        m_current_sections.reserve(16);
        m_onelevel_indent_string = ""; // using zero space for indentation is just to save disk space
        m_json_pretty_print = false;
        if (!json_file_prefix.empty())
            init_json_output_file(json_file_prefix);
    }

    ~CMonitorOutputFrontend() { close(); }

    //------------------------------------------------------------------------------
    // setup API
    //------------------------------------------------------------------------------

    void init_json_output_file(const std::string& filenamePrefix);
    void init_influxdb_connection(const std::string& hostname, unsigned int port, const std::string& dbname);
    void init_prometheus_connection(const std::string& port);
    void enable_json_pretty_print();
    void close();

    //------------------------------------------------------------------------------
    // Sample/Section/Subsection
    //------------------------------------------------------------------------------

    void pheader_start();
    void psample_start();

    void psample_array_start();
    void psample_array_end();

    void psection_start(const char* section);
    void psection_end();

    void psubsection_start(const char* resource);
    void psubsection_end();

    //------------------------------------------------------------------------------
    // Measurement creation:
    //------------------------------------------------------------------------------

    void phex(const char* name, long long value);
    void plong(const char* name, long long value);
    void pdouble(const char* name, double value);
    void pstring(const char* name, const char* value);

    void pstats();

    //------------------------------------------------------------------------------
    // Current sample manipulation:
    //------------------------------------------------------------------------------

    size_t get_current_sample_measurements() const;
    void push_header() { push_current_sections(true); } // writes on file, stdout or socket
    void push_current_sample() { push_current_sections(false); } // writes on file, stdout or socket

private:
    class CMonitorOutputMeasurement {
    public:
        CMonitorOutputMeasurement(const char* name = "", const char* value = "", bool numeric = false)
        {
            strncpy(m_name.data(), name, CMONITOR_MEASUREMENT_NAME_MAXLEN - 1);
            strncpy(m_value.data(), value, CMONITOR_MEASUREMENT_VALUE_MAXLEN - 1);
            m_numeric = numeric;
        }

        std::array<char, CMONITOR_MEASUREMENT_NAME_MAXLEN> m_name; // use std::array to void dynamic allocations
        std::array<char, CMONITOR_MEASUREMENT_VALUE_MAXLEN> m_value; // use std::array to void dynamic allocations
        bool m_numeric;
    };

    typedef std::vector<CMonitorOutputMeasurement> CMonitorMeasurementVector;

    class CMonitorOutputSubsection {
    public:
        std::string m_name;
        CMonitorMeasurementVector m_measurements;

        std::string get_value_for_measurement(const std::string& name) const
        {
            for (const auto& m : m_measurements)
                if (strncmp(m.m_name.data(), name.c_str(), m.m_name.size()) == 0)
                    return std::string(m.m_value.data());
            return "";
        }
    };

    class CMonitorOutputSection {
    public:
        std::string m_name;
        std::vector<CMonitorOutputSubsection> m_subsections;
        CMonitorMeasurementVector m_measurements;

        std::string get_value_for_measurement(const std::string& name) const
        {
            for (const auto& m : m_measurements)
                if (strncmp(m.m_name.data(), name.c_str(), m.m_name.size()) == 0)
                    return std::string(m.m_value.data());
            return "";
        }
    };

    //------------------------------------------------------------------------------
    // JSON low-level functions
    //------------------------------------------------------------------------------

    void push_json_indent(unsigned int indent);
    void push_json_measurements(CMonitorMeasurementVector& measurements, unsigned int indent);
    void push_json_object_start(const std::string& str, unsigned int indent);
    void push_json_object_end(bool last, unsigned int indent);
    void push_json_array_start(const std::string& str, unsigned int indent);
    void push_json_array_end(unsigned int indent);
    void push_current_sections_to_json(bool is_header);

    //------------------------------------------------------------------------------
    // InfluxDB low-level functions
    //------------------------------------------------------------------------------

    static bool contains_char_to_escape(const char* string);
    static void get_quoted_field_value(std::string& out, const char* value);
    static void get_quoted_tag_value(std::string& out, const char* value);

    std::string generate_influxdb_line(
        CMonitorMeasurementVector& measurements, const std::string& meas_name, const std::string& ts_nsec);

    void push_current_sections_to_influxdb(bool is_header);

    //------------------------------------------------------------------------------
    // Prometheus low-level functions
    //------------------------------------------------------------------------------
    void push_current_sections_to_prometheus();
    void generate_prometheus_metric(const std::string& metric_name, const std::string& metric_data, const std::string& metric_value);

    // main output routine:
    void push_current_sections(bool is_header);

private:
    // Structured measurements generated so far for last sample:
    std::vector<CMonitorOutputSection> m_current_sections;
    CMonitorMeasurementVector* m_current_meas_list
        = nullptr; // pointer to current CMonitorMeasurementVector inside m_current_sections

    // InfluxDB internals
    influx_client_t* m_influxdb_client_conn = nullptr;
    std::string m_influxdb_tagset;

    // Prometheus exposer
     bool m_prometheusEnabled = false;
     std::unique_ptr<prometheus::Exposer> m_exposer;
     std::shared_ptr<prometheus::Registry> m_prometheus_registry = std::make_shared<prometheus::Registry>();

    // JSON internals
    FILE* m_outputJson = nullptr;
    std::string m_onelevel_indent_string;
    bool m_json_pretty_print = false;

    // Stats on the generated output
    unsigned int m_samples = 0;
    unsigned int m_sections = 0;
    unsigned int m_subsections = 0;
    unsigned int m_string = 0;
    unsigned int m_long = 0;
    unsigned int m_double = 0;
    unsigned int m_hex = 0;
};
