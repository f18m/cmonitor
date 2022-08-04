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

#include "system.h"

// Prometheus
#ifdef PROMETHEUS_SUPPORT
#include "prometheus_counter.h"
#include "prometheus_gauge.h"
#include <prometheus/counter.h>
#include <prometheus/detail/future_std.h>
#include <prometheus/exposer.h>
#include <prometheus/family.h>
#include <prometheus/gauge.h>
#include <prometheus/labels.h>
#include <prometheus/registry.h>
#endif

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
    void enable_json_pretty_print();
    void close();

#ifdef PROMETHEUS_SUPPORT
    void init_prometheus_connection(const std::string& port, const std::map<std::string, std::string>& metaData = {});
    void init_prometheus_kpi(const prometheus_kpi_descriptor* kpi, size_t size);
    bool is_prometheus_enabled() { return m_prometheus_enabled; }
#endif

    //------------------------------------------------------------------------------
    // Sample/Section/Subsection
    //------------------------------------------------------------------------------

    void pheader_start();
    void psample_start();

    void psample_array_start();
    void psample_array_end();

    void psection_start(const char* section);
    void psection_end();

    void psubsection_start(const char* resource, const std::map<std::string, std::string>& labels = {});
    void psubsection_end();

    void psubsubsection_start(const char* resource, const std::map<std::string, std::string>& labels = {});
    void psubsubsection_end();

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
        CMonitorOutputMeasurement(const char* name = "", const char* value = "")
        {
            strncpy(m_name.data(), name, CMONITOR_MEASUREMENT_NAME_MAXLEN - 1);
            strncpy(m_value.data(), value, CMONITOR_MEASUREMENT_VALUE_MAXLEN - 1);
            m_dvalue = 0;
            m_numeric = false;
        }
        CMonitorOutputMeasurement(const char* name, double value)
        {
            strncpy(m_name.data(), name, CMONITOR_MEASUREMENT_NAME_MAXLEN - 1);
            // FIXME1: we should find a way to format directly into m_value!
            // FIXME:2 we should do the double -> string conversion only when JSON output is enabled; e.g. Prometheus
            // output frontend does not need m_value conversion; or even better we should remove entirely m_value and do
            // the conversion on the fly ONLY when producing JSON output

            // with std::to_string() you cannot specify the accuracy (how many decimal digits)
            auto tmp = fmt::format("{:.3f}", value);
            strncpy(m_value.data(), tmp.c_str(), CMONITOR_MEASUREMENT_VALUE_MAXLEN - 1);
            m_dvalue = value;
            m_numeric = true;
        }
        CMonitorOutputMeasurement(const char* name, long long value)
        {
            strncpy(m_name.data(), name, CMONITOR_MEASUREMENT_NAME_MAXLEN - 1);
            // FIXME1: we should find a way to format directly into m_value!
            // FIXME:2 we should do the double -> string conversion only when JSON output is enabled; e.g. Prometheus
            // output frontend does not need m_value conversion; or even better we should remove entirely m_value and do
            // the conversion on the fly ONLY when producing JSON output

            // according to https://www.zverovich.net/2020/06/13/fast-int-to-string-revisited.html
            // fmt::format_int is be the fastest way to convert integers
#if FMTLIB_MAJOR_VER >= 6
            auto tmp = fmt::format_int(value);
#else
            auto tmp = fmt::format("{}", value);
#endif
            strncpy(m_value.data(), tmp.c_str(), CMONITOR_MEASUREMENT_VALUE_MAXLEN - 1);
            m_dvalue = value;
            m_numeric = true;
        }

        void enforce_valid_json_string_value()
        {
            char* p = &m_value[0];
            while (*p != '\0') {
                // isgraph() returns != 0 for following chars:
                //  !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~
                // which are all valid in JSON output, except for the \ character which should be repeated twice to
                // escape it; however we don't care about that and replace it with space if it appears for some reason
                // Same thing is done for the double quotes " character since we use to enclose
                if (*p != ' ' && (isgraph(*p) == 0 || *p == '\\' || *p == '"')) {
                    *p = '*';
                }

                p++;
            }
        }

        std::array<char, CMONITOR_MEASUREMENT_NAME_MAXLEN> m_name; // use std::array to void dynamic allocations
        std::array<char, CMONITOR_MEASUREMENT_VALUE_MAXLEN> m_value; // use std::array to void dynamic allocations
        double m_dvalue = 0; // double value to avoid atof conversion
        bool m_numeric;
    };

    typedef std::vector<CMonitorOutputMeasurement> CMonitorMeasurementVector;

    class CMonitorOutputSubSubsection {
    public:
        std::string m_name;
        std::map<std::string, std::string> m_labels;
        CMonitorMeasurementVector m_measurements;

        std::string get_value_for_measurement(const std::string& name) const
        {
            for (const auto& m : m_measurements)
                if (strncmp(m.m_name.data(), name.c_str(), m.m_name.size()) == 0)
                    return std::string(m.m_value.data());
            return "";
        }
    };

    class CMonitorOutputSubsection {
    public:
        std::string m_name;
        std::map<std::string, std::string> m_labels;
        std::vector<CMonitorOutputSubSubsection> m_subsubsections;
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

    // main output routine:
    void push_current_sections(bool is_header);

//------------------------------------------------------------------------------
// Prometheus low-level functions
//------------------------------------------------------------------------------
#ifdef PROMETHEUS_SUPPORT
    void push_current_sections_to_prometheus();
    void generate_prometheus_metric(const std::string& metric_name, const std::string& metric_data, double metric_value,
        const std::map<std::string, std::string>& labels = {});
#endif

private:
    // Structured measurements generated so far for last sample:
    std::vector<CMonitorOutputSection> m_current_sections;
    CMonitorMeasurementVector* m_current_meas_list
        = nullptr; // pointer to current CMonitorMeasurementVector inside m_current_sections

    // InfluxDB internals
    influx_client_t* m_influxdb_client_conn = nullptr;
    std::string m_influxdb_tagset;

    // JSON internals
    FILE* m_outputJson = nullptr;
    std::string m_onelevel_indent_string;
    bool m_json_pretty_print = false;

// Prometheus exposer
#ifdef PROMETHEUS_SUPPORT
    bool m_prometheus_enabled = false;
    std::unique_ptr<prometheus::Exposer> m_prometheus_exposer;
    std::shared_ptr<prometheus::Registry> m_prometheus_registry;
    std::map<std::string, PrometheusKpi*> m_prometheus_kpi_map;
    std::map<std::string, std::string> m_default_labels;
#endif

    // Stats on the generated output
    unsigned int m_samples = 0;
    unsigned int m_sections = 0;
    unsigned int m_subsections = 0;
    unsigned int m_subsubsections = 0;
    unsigned int m_string = 0;
    unsigned int m_long = 0;
    unsigned int m_double = 0;
    unsigned int m_hex = 0;
};
