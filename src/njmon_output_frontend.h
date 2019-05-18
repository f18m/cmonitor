#pragma once

#include <array>
#include <set>
#include <string.h>
#include <string>
#include <vector>

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

class NjmonOutputFrontend {
public:
    NjmonOutputFrontend() { m_current_sections.reserve(16); }

    //------------------------------------------------------------------------------
    // setup API
    //------------------------------------------------------------------------------

    void init_json_output_file(const std::string& filenamePrefix);
    void init_influxdb_connection(const std::string& hostname, unsigned int port);

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
    class NjmonOutputMeasurement {
    public:
        NjmonOutputMeasurement(const char* name = "", const char* value = "", bool numeric = false)
        {
            strncpy(m_name.data(), name, 64);
            strncpy(m_value.data(), value, 64);
            m_numeric = numeric;
        }

        std::array<char, 64> m_name; // use std::array to void dynamic allocations
        std::array<char, 64> m_value; // use std::array to void dynamic allocations
        bool m_numeric;
    };

    typedef std::vector<NjmonOutputMeasurement> NjmonMeasurementVector;

    class NjmonOutputSubsection {
    public:
        std::string m_name;
        NjmonMeasurementVector m_measurements;
    };

    class NjmonOutputSection {
    public:
        std::string m_name;
        std::vector<NjmonOutputSubsection> m_subsections;
        NjmonMeasurementVector m_measurements;
    };

    //------------------------------------------------------------------------------
    // JSON low-level functions
    //------------------------------------------------------------------------------

    void push_json_indent(unsigned int indent);
    void push_json_measurements(NjmonMeasurementVector& measurements, unsigned int indent);
    void push_json_object_start(const std::string& str, unsigned int indent);
    void push_json_object_end(bool last, unsigned int indent);

    //------------------------------------------------------------------------------
    // InfluxDB low-level functions
    //------------------------------------------------------------------------------

    std::string generate_influxdb_line(
        NjmonMeasurementVector& measurements, const std::string& meas_name, const std::string& ts_nsec);

    void push_current_sections(bool is_header);

private:
    // Structured measurements generated so far for last sample:
    std::vector<NjmonOutputSection> m_current_sections;
    NjmonMeasurementVector* m_current_meas_list = nullptr;

    // InfluxDB internals
    influx_client_t* m_influxdb_client_conn = nullptr;

    // JSON internals
    FILE* m_outputJson = nullptr;

    // Stats on the generated output
    unsigned int m_njmon_samples = 0;
    unsigned int m_njmon_sections = 0;
    unsigned int m_njmon_subsections = 0;
    unsigned int m_njmon_string = 0;
    unsigned int m_njmon_long = 0;
    unsigned int m_njmon_double = 0;
    unsigned int m_njmon_hex = 0;
};

extern NjmonOutputFrontend g_output;
