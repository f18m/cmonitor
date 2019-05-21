/*
 * njmon_output_frontend.cpp -- routines to generate the JSON output and/or
 *                              generate the InfluxDB measurements and send them
 *                              over TCP/UDP socket
 * Developer: Nigel Griffiths, Francesco Montorsi
 * (C) Copyright 2018 Nigel Griffiths, Francesco Montorsi

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

#include "influxdb.h"
#include "njmon.h"
#include <algorithm>
#include <assert.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

NjmonOutputFrontend g_output;

//------------------------------------------------------------------------------
// Init functions
//------------------------------------------------------------------------------

void NjmonOutputFrontend::init_json_output_file(const std::string& filenamePrefix)
{
    if (filenamePrefix == "stdout") {
        // open stdout as FILE*
        if ((m_outputJson = fdopen(STDOUT_FILENO, "w")) == 0) {
            perror("opening stdout for write");
            exit(13);
        }
    } else if (filenamePrefix == "none") {
        m_outputJson = nullptr;
        g_logger.LogDebug("Disabled JSON generation (filename prefix = none)");
        printf("Disabling JSON file generation (collected data will be available only via InfluxDB, if IP/port is "
               "provided)\n");
    } else {
        // open output files
        char filename[1024];
        sprintf(filename, "%s.json", filenamePrefix.c_str());
        if ((m_outputJson = fopen(filename, "w")) == 0) {
            perror("opening file for stdout");
            fprintf(stderr, "ERROR nmon filename=%s\n", filename);
            exit(13);
        }

        printf("Opened output JSON file '%s'\n", filename);
    }
}

std::string hostname_to_ip(const std::string& hostname)
{
    struct hostent* he;
    struct in_addr** addr_list;
    std::string ip;
    int i;

    if ((he = gethostbyname(hostname.c_str())) == NULL)
        return "";

    addr_list = (struct in_addr**)he->h_addr_list;
    for (i = 0; addr_list[i] != NULL; i++) {
        // Return the first one;
        ip = inet_ntoa(*addr_list[i]);
        return ip;
    }

    return "";
}

void NjmonOutputFrontend::init_influxdb_connection(const std::string& hostname, unsigned int port)
{
    std::string ipaddress = hostname_to_ip(hostname);
    if (ipaddress.empty()) {
        char buf[1024];
        herror(buf);
        fprintf(stderr, "hostname=%s to IP address convertion failed, bailing out: %s\n", hostname.c_str(), buf);
        exit(98);
    }

    m_influxdb_client_conn = new influx_client_t();
    m_influxdb_client_conn->host = strdup(ipaddress.c_str()); // force newline
    m_influxdb_client_conn->port = port; // force newline
    m_influxdb_client_conn->db = strdup("njmon"); // force newline
    m_influxdb_client_conn->usr = strdup("usr"); // force newline
    m_influxdb_client_conn->pwd = strdup("pwd"); // force newline

    g_logger.LogDebug("init_influxdb_connection() initialized InfluxDB connection to %s:%d",
        m_influxdb_client_conn->host, m_influxdb_client_conn->port);
}

//------------------------------------------------------------------------------
// Low level InfluxDB functions
//------------------------------------------------------------------------------

/* static */
bool NjmonOutputFrontend::contains_char_to_escape(const char* string)
{
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/
    const char* special_chars = ",= \"";
    if (strpbrk(string, special_chars))
        return true;
    return false;
}

/* static */
void NjmonOutputFrontend::get_quoted_field_value(std::string& out, const char* value)
{
    out.clear();
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/
    // For string field values use a backslash character \ to escape:
    // - double quotes
    for (const char* p = value; *p != '\0'; p++) {
        if (*p == '"') {
            out += "\\\"";
        } else
            out += *p;
    }
}

/* static */
void NjmonOutputFrontend::get_quoted_tag_value(std::string& out, const char* value)
{
    out.clear();

    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/
    // For tag keys, tag values, and field keys always use a backslash character \ to escape:
    // - commas
    // - equal signs
    // - spaces

    for (const char* p = value; *p != '\0'; p++) {
        if (*p == ',') {
            out += "\\,";
        } else if (*p == '=') {
            out += "\\=";
        } else if (*p == ' ') {
            out += "\\ ";
        } else
            out += *p;
    }
}

std::string NjmonOutputFrontend::generate_influxdb_line(
    NjmonMeasurementVector& measurements, const std::string& meas_name, const std::string& ts_nsec)
{
    // format data according to the InfluxDB "line protocol":
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/

    g_logger.LogDebug("generate_influxdb_line() generating measurement: %s\n", meas_name.c_str());

    std::string ret;
    ret.reserve(1024);

    // Measurement
    assert(!contains_char_to_escape(meas_name.c_str()));
    ret += meas_name;

    // Tag set
    // NOTE: unlike fields, tags are indexed and can be used to tag measurements and allow to search
    //       for them:
    ret += m_influxdb_tagset;

    // Whitespace I
    ret += " ";

    // Field set
    std::string tmp;
    tmp.reserve(256);
    for (size_t n = 0; n < measurements.size(); n++) {
        auto& m = measurements[n];

        // Field Name
        assert(!contains_char_to_escape(m.m_name.data()));
        ret += m.m_name.data();

        ret += "=";

        // Field Value
        if (m.m_numeric) {
            ret += m.m_value.data();
        } else {
            get_quoted_field_value(tmp, m.m_value.data());

            ret += "\"";
            ret += tmp;
            ret += "\"";
        }

        if (n < measurements.size() - 1)
            ret += ",";
    }

    // Whitespace II
    ret += " ";

    // Timestamp
    ret += ts_nsec;

    return ret;
}

void NjmonOutputFrontend::push_current_sections_to_influxdb(bool is_header)
{
    if (is_header) {
        // instead of actually pushing something towards the InfluxDB server, generate the tagsets:

        // collect tags
        std::vector<std::pair<std::string /* tag name */, std::string /* tag value */>> tags;
        for (auto& sec : m_current_sections) {
            if (sec.m_name == "identity") {
                tags.push_back(std::make_pair("hostname", sec.get_value_for_measurement("hostname")));

                std::string ips = sec.get_value_for_measurement("all_ip_addresses");
                replace_string(ips, ",", " ", true);
                tags.push_back(std::make_pair("all_ip_addresses", ips));
            } else if (sec.m_name == "os_release") {
                tags.push_back(std::make_pair("os_name", sec.get_value_for_measurement("name")));
                tags.push_back(std::make_pair("os_pretty_name", sec.get_value_for_measurement("pretty_name")));
            } else if (sec.m_name == "cgroup_config") {
                tags.push_back(std::make_pair("cgroup_name", sec.get_value_for_measurement("name")));
            } else if (sec.m_name == "lscpu") {
                tags.push_back(std::make_pair("cpu_model_name", sec.get_value_for_measurement("model_name")));
            }
        }

        // now prepare the tagset line according to the protocol:
        //  https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/

        std::string tmp;
        for (auto& tag : tags) {
            assert(!contains_char_to_escape(tag.first.c_str()));
            get_quoted_tag_value(tmp, tag.second.c_str());

            m_influxdb_tagset += tag.first + "=" + tmp + ",";
        }

        if (!m_influxdb_tagset.empty())
            m_influxdb_tagset.pop_back();

        g_logger.LogDebug(
            "push_current_sections_to_influxdb() generated tagset for InfluxDB:\n %s\n", m_influxdb_tagset.c_str());

    } else {
        struct timeval tv;
        gettimeofday(&tv, 0);
        uint64_t ts_nsec = ((uint64_t)tv.tv_sec * 1E9) + ((uint64_t)tv.tv_usec * 1E3);

        char ts_nsec_str[64];
        snprintf(ts_nsec_str, sizeof(ts_nsec_str), "%lu", ts_nsec);

        std::string all_measurements;
        all_measurements.reserve(4096);
        for (size_t sec_idx = 0; sec_idx < m_current_sections.size(); sec_idx++) {
            auto& sec = m_current_sections[sec_idx];
            if (sec.m_measurements.empty()) {

                for (size_t subsec_idx = 0; subsec_idx < sec.m_subsections.size(); subsec_idx++) {
                    auto& subsec = sec.m_subsections[subsec_idx];
                    all_measurements
                        += generate_influxdb_line(subsec.m_measurements, sec.m_name + "_" + subsec.m_name, ts_nsec_str);

                    if (subsec_idx < sec.m_subsections.size() - 1)
                        all_measurements += "\n";
                }

            } else {
                all_measurements += generate_influxdb_line(sec.m_measurements, sec.m_name, ts_nsec_str);
            }

            if (sec_idx < m_current_sections.size() - 1)
                all_measurements += "\n";
        }

        size_t num_measurements = get_current_sample_measurements();
        g_logger.LogDebug(
            "push_current_sections_to_influxdb() pushing to InfluxDB %zu measurements for timestamp: %s\n",
            num_measurements, ts_nsec_str);

        post_http_send_line(m_influxdb_client_conn, all_measurements.data(), all_measurements.size());
    }
}

//------------------------------------------------------------------------------
// Low level JSON functions
//------------------------------------------------------------------------------

void NjmonOutputFrontend::push_json_indent(unsigned int indent)
{
    for (size_t i = 0; i < indent; i++)
        fputs("     ", m_outputJson);
}

void NjmonOutputFrontend::push_json_measurements(NjmonMeasurementVector& measurements, unsigned int indent)
{
    for (size_t n = 0; n < measurements.size(); n++) {
        auto& m = measurements[n];

        push_json_indent(indent);

        fputs("\"", m_outputJson);
        fputs(m.m_name.data(), m_outputJson);
        if (m.m_numeric) {
            fputs("\": ", m_outputJson);
            fputs(m.m_value.data(), m_outputJson);
        } else {
            fputs("\": \"", m_outputJson);
            fputs(m.m_value.data(), m_outputJson);
            fputs("\"", m_outputJson);
        }

        bool last = (n == measurements.size() - 1);
        if (last)
            fputs("\n", m_outputJson);
        else
            fputs(",\n", m_outputJson);
    }
}

void NjmonOutputFrontend::push_json_object_start(const std::string& str, unsigned int indent)
{
    push_json_indent(indent);
    fputs("\"", m_outputJson);
    fwrite(str.c_str(), 1, str.size(), m_outputJson);
    fputs("\": {\n", m_outputJson);
}

void NjmonOutputFrontend::push_json_object_end(bool last, unsigned int indent)
{
    push_json_indent(indent);
    if (last)
        fputs("}\n", m_outputJson);
    else
        fputs("},\n", m_outputJson);
}

void NjmonOutputFrontend::push_current_sections_to_json(bool is_header)
{
    // convert the current sample into JSON format:

    if (is_header) {
        fputs("{\n", m_outputJson); // document begin
        fputs("    \"header\": {\n", m_outputJson);
    } else {
        if (m_njmon_samples > 0)
            fputs(",\n", m_outputJson); // add separator from previous sample
        fputs("    {\n", m_outputJson); // start of new sample inside sample array
    }
    for (size_t sec_idx = 0; sec_idx < m_current_sections.size(); sec_idx++) {
        auto& sec = m_current_sections[sec_idx];

        push_json_object_start(sec.m_name, 2);
        if (sec.m_measurements.empty()) {
            for (size_t subsec_idx = 0; subsec_idx < sec.m_subsections.size(); subsec_idx++) {
                auto& subsec = sec.m_subsections[subsec_idx];

                push_json_object_start(subsec.m_name, 3);
                push_json_measurements(subsec.m_measurements, 4);
                push_json_object_end(subsec_idx == sec.m_subsections.size() - 1, 3);
            }
        } else {
            push_json_measurements(sec.m_measurements, 3);
        }
        push_json_object_end(sec_idx == m_current_sections.size() - 1, 2);
    }
    if (is_header) {
        fputs("    },\n", m_outputJson); // for sure at least 1 sample will follow
    } else {
        fputs("    }", m_outputJson); // not sure if more samples will follow
        m_njmon_samples++;
    }

    size_t num_measurements = get_current_sample_measurements();
    g_logger.LogDebug(
        "push_current_sections_to_json() writing on the JSON output %lu measurements\n", num_measurements);
}

//------------------------------------------------------------------------------
// Generic routines
//------------------------------------------------------------------------------

void NjmonOutputFrontend::push_current_sections(bool is_header)
{
    DEBUGLOG_FUNCTION_START();

    if (m_outputJson)
        push_current_sections_to_json(is_header);

    if (m_influxdb_client_conn)
        push_current_sections_to_influxdb(is_header);

    fflush(NULL); /* force I/O output now */

    // IMPORTANT: clear() but do not shrink_to_fit() to avoid a bunch of reallocations for next sample:
    m_current_sections.clear();
}

size_t NjmonOutputFrontend::get_current_sample_measurements() const
{
    size_t ntotal_meas = 0;
    for (size_t i = 0; i < m_current_sections.size(); i++) {
        auto& sec = m_current_sections[i];
        if (sec.m_measurements.empty()) {
            for (size_t i = 0; i < sec.m_subsections.size(); i++) {
                auto& subsec = sec.m_subsections[i];
                ntotal_meas += subsec.m_measurements.size();
            }
        } else {
            ntotal_meas += sec.m_measurements.size();
        }
    }

    return ntotal_meas;
}

//------------------------------------------------------------------------------
// JSON objects
//------------------------------------------------------------------------------

void NjmonOutputFrontend::pstats()
{
    psection_start("njmon_stats");
    plong("section", m_njmon_sections);
    plong("subsections", m_njmon_subsections);
    plong("string", m_njmon_string);
    plong("long", m_njmon_long);
    plong("double", m_njmon_double);
    plong("hex", m_njmon_hex);
    psection_end();
}

void NjmonOutputFrontend::pheader_start()
{
    // empty for now
}

void NjmonOutputFrontend::psample_array_start()
{
    if (m_outputJson) {
        fputs("    \"samples\": [\n", m_outputJson);
    }
}

void NjmonOutputFrontend::psample_array_end()
{
    if (m_outputJson) {
        fputs("]\n", m_outputJson);
        fputs("}\n", m_outputJson);
    }
}

void NjmonOutputFrontend::psample_start()
{
    // empty for now
}

void NjmonOutputFrontend::psection_start(const char* section)
{
    m_njmon_sections++;

    NjmonOutputSection sec;
    sec.m_name = section;
    m_current_sections.push_back(sec);

    // when adding new measurements, add them as children of this new section:
    m_current_meas_list = &m_current_sections.back().m_measurements;
}

void NjmonOutputFrontend::psection_end()
{
    // stop adding measurements to last section:
    m_current_meas_list = nullptr;
}

void NjmonOutputFrontend::psubsection_start(const char* resource)
{
    m_njmon_subsections++;

    NjmonOutputSubsection sec;
    sec.m_name = resource;
    m_current_sections.back().m_subsections.push_back(sec);

    // when adding new measurements, add them as children of this new subsection:
    m_current_meas_list = &m_current_sections.back().m_subsections.back().m_measurements;
}

void NjmonOutputFrontend::psubsection_end()
{
    // stop adding measurements to last subsection:
    m_current_meas_list = nullptr;
}

//------------------------------------------------------------------------------
// JSON field/values
//------------------------------------------------------------------------------

void NjmonOutputFrontend::phex(const char* name, long long value)
{
    m_njmon_hex++;
    assert(m_current_meas_list);

    char buff[128];
    snprintf(buff, sizeof(buff), "0x%08llx", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
}

void NjmonOutputFrontend::plong(const char* name, long long value)
{
    m_njmon_long++;
    assert(m_current_meas_list);

    char buff[128];
    snprintf(buff, sizeof(buff), "%lld", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
}

void NjmonOutputFrontend::pdouble(const char* name, double value)
{
    m_njmon_double++;
    assert(m_current_meas_list);

    char buff[128];
    snprintf(buff, sizeof(buff), "%.3f", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
}

void NjmonOutputFrontend::pstring(const char* name, const char* value)
{
    m_njmon_string++;
    assert(m_current_meas_list);

    m_current_meas_list->push_back(NjmonOutputMeasurement(name, value));
}
