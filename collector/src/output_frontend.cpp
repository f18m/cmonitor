/*
 * output_frontend.cpp -- routines to generate the JSON output and/or
 *                        generate the InfluxDB measurements and send them
 *                        over TCP/UDP socket
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

#include "output_frontend.h"
#include "cmonitor.h"
#include "influxdb.h"
#include "logger.h"
#include "utils.h"
#include <algorithm>
#include <assert.h>
#include <fmt/format.h>
#include <netdb.h>
#include <sys/time.h>
#include <unistd.h>

//------------------------------------------------------------------------------
// Init functions
//------------------------------------------------------------------------------

void CMonitorOutputFrontend::close()
{
    if (m_outputJson) {
        fclose(m_outputJson);
        m_outputJson = nullptr;
    }
    if (m_influxdb_client_conn) {
        delete m_influxdb_client_conn;
        m_influxdb_client_conn = nullptr;
    }
}

void CMonitorOutputFrontend::init_json_output_file(const std::string& filenamePrefix)
{
    if (filenamePrefix == "stdout") {
        // open stdout as FILE*
        if ((m_outputJson = fdopen(STDOUT_FILENO, "w")) == 0) {
            perror("opening stdout for write");
            exit(13);
        }
    } else if (filenamePrefix == "none") {
        m_outputJson = nullptr;
        CMonitorLogger::instance()->LogDebug("Disabled JSON generation (filename prefix = none)");
        printf("Disabling JSON file generation (collected data will be available only via InfluxDB, if IP/port is "
               "provided)\n");
    } else {

        std::string outFile(filenamePrefix);
        if (filenamePrefix.size() > 5 && filenamePrefix.substr(filenamePrefix.size() - 5) != ".json")
            outFile += ".json";

        // open output files
        if ((m_outputJson = fopen(outFile.c_str(), "w")) == 0) {
            perror("opening file for stdout");
            fprintf(stderr, "ERROR nmon filename=%s\n", outFile.c_str());
            exit(13);
        }

        printf("Opened output JSON file '%s'\n", outFile.c_str());
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

void CMonitorOutputFrontend::init_influxdb_connection(
    const std::string& hostname, unsigned int port, const std::string& dbname)
{
    std::string ipaddress = hostname_to_ip(hostname);
    if (ipaddress.empty()) {
        char buf[1024];
        herror(buf);
        fprintf(stderr, "Lookup of IP address for hostname %s failed, bailing out: %s\n", hostname.c_str(), buf);
        exit(98);
    }

    m_influxdb_client_conn = new influx_client_t();
    m_influxdb_client_conn->host = strdup(ipaddress.c_str()); // force newline
    m_influxdb_client_conn->port = port; // force newline
    m_influxdb_client_conn->db = strdup(dbname.c_str()); // force newline
    m_influxdb_client_conn->usr = strdup("usr"); // force newline
    m_influxdb_client_conn->pwd = strdup("pwd"); // force newline

    CMonitorLogger::instance()->LogDebug("init_influxdb_connection() initialized InfluxDB connection to %s:%d",
        m_influxdb_client_conn->host, m_influxdb_client_conn->port);
}

void CMonitorOutputFrontend::enable_json_pretty_print()
{
    m_onelevel_indent_string = "    ";
    m_json_pretty_print = true;
    CMonitorLogger::instance()->LogDebug("Enabling pretty printing of the JSON");
}

//------------------------------------------------------------------------------
// Low level InfluxDB functions
//------------------------------------------------------------------------------

/* static */
bool CMonitorOutputFrontend::contains_char_to_escape(const char* string)
{
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/
    const char* special_chars = ",= \"";
    if (strpbrk(string, special_chars))
        return true;
    return false;
}

/* static */
void CMonitorOutputFrontend::get_quoted_field_value(std::string& out, const char* value)
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
void CMonitorOutputFrontend::get_quoted_tag_value(std::string& out, const char* value)
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

std::string CMonitorOutputFrontend::generate_influxdb_line(
    CMonitorMeasurementVector& measurements, const std::string& meas_name, const std::string& ts_nsec)
{
    // format data according to the InfluxDB "line protocol":
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/

    CMonitorLogger::instance()->LogDebug("generate_influxdb_line() generating measurement: %s\n", meas_name.c_str());

    std::string ret;
    ret.reserve(1024);

    // Measurement
    assert(!contains_char_to_escape(meas_name.c_str()));
    ret += meas_name;

    // Tag set
    // NOTE: unlike fields, tags are indexed and can be used to tag measurements and allow to search
    //       for them:
    if (!m_influxdb_tagset.empty())
        ret += "," + m_influxdb_tagset;

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

void CMonitorOutputFrontend::push_current_sections_to_influxdb(bool is_header)
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

        CMonitorLogger::instance()->LogDebug(
            "push_current_sections_to_influxdb() generated tagset for InfluxDB:\n %s\n", m_influxdb_tagset.c_str());

    } else {
        struct timeval tv;
        gettimeofday(&tv, 0);
        uint64_t ts_nsec = ((uint64_t)tv.tv_sec * 1E9) + ((uint64_t)tv.tv_usec * 1E3);

        std::string ts_nsec_str = fmt::format_int(ts_nsec).str();

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
        CMonitorLogger::instance()->LogDebug(
            "push_current_sections_to_influxdb() pushing to InfluxDB %zu measurements for timestamp: %s\n",
            num_measurements, ts_nsec_str.c_str());

        post_http_send_line(m_influxdb_client_conn, all_measurements.data(), all_measurements.size());
    }
}

//------------------------------------------------------------------------------
// Low level JSON functions
//------------------------------------------------------------------------------

void CMonitorOutputFrontend::push_json_indent(unsigned int indent)
{
    if (m_onelevel_indent_string.empty())
        return;

    for (size_t i = 0; i < indent; i++)
        fputs(m_onelevel_indent_string.c_str(), m_outputJson);
}

void CMonitorOutputFrontend::push_json_measurements(CMonitorMeasurementVector& measurements, unsigned int indent)
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
        if (!last)
            fputs(",", m_outputJson);

        if (m_json_pretty_print)
            fputs("\n", m_outputJson);
    }
}

void CMonitorOutputFrontend::push_json_object_start(const std::string& str, unsigned int indent)
{
    push_json_indent(indent);
    fputs("\"", m_outputJson);
    fwrite(str.c_str(), 1, str.size(), m_outputJson);
    fputs("\": {", m_outputJson);

    if (m_json_pretty_print)
        fputs("\n", m_outputJson);
}

void CMonitorOutputFrontend::push_json_object_end(bool last, unsigned int indent)
{
    push_json_indent(indent);
    if (last)
        fputs("}", m_outputJson);
    else
        fputs("},", m_outputJson);

    if (m_json_pretty_print)
        fputs("\n", m_outputJson);
}

void CMonitorOutputFrontend::push_json_array_start(const std::string& str, unsigned int indent)
{
    push_json_indent(indent);
    fputs("\"", m_outputJson);
    fwrite(str.c_str(), 1, str.size(), m_outputJson);
    fputs("\": [\n", m_outputJson);
}

void CMonitorOutputFrontend::push_json_array_end(unsigned int indent)
{
    push_json_indent(indent);

    fputs("]\n", m_outputJson);
    fputs("}\n", m_outputJson);
}

void CMonitorOutputFrontend::push_current_sections_to_json(bool is_header)
{
    // convert the current sample into JSON format:

    // we do all the JSON with max 4 indentation levels:
    enum { FIRST_LEVEL = 1, SECOND_LEVEL = 2, THIRD_LEVEL = 3, FOURTH_LEVEL = 4 };

    if (is_header) {
        fputs("{\n", m_outputJson); // document begin
        push_json_object_start("header", FIRST_LEVEL);
    } else {
        if (m_samples > 0)
            fputs(",\n", m_outputJson); // add separator from previous sample
        push_json_indent(FIRST_LEVEL);
        fputs("{", m_outputJson); // start of new sample inside sample array
        if (m_json_pretty_print)
            fputs("\n", m_outputJson);
    }
    for (size_t sec_idx = 0; sec_idx < m_current_sections.size(); sec_idx++) {
        auto& sec = m_current_sections[sec_idx];

        push_json_object_start(sec.m_name, SECOND_LEVEL);
        if (sec.m_measurements.empty()) {
            for (size_t subsec_idx = 0; subsec_idx < sec.m_subsections.size(); subsec_idx++) {
                auto& subsec = sec.m_subsections[subsec_idx];

                push_json_object_start(subsec.m_name, THIRD_LEVEL);
                push_json_measurements(subsec.m_measurements, FOURTH_LEVEL);
                push_json_object_end(subsec_idx == sec.m_subsections.size() - 1, THIRD_LEVEL);
            }
        } else {
            push_json_measurements(sec.m_measurements, THIRD_LEVEL);
        }
        push_json_object_end(sec_idx == m_current_sections.size() - 1, SECOND_LEVEL);
    }
    if (is_header) {
        push_json_indent(FIRST_LEVEL);
        fputs("},\n", m_outputJson); // for sure at least 1 sample will follow
    } else {
        push_json_indent(FIRST_LEVEL);
        fputs("}", m_outputJson); // not sure if more samples will follow
        m_samples++;
    }

    size_t num_measurements = get_current_sample_measurements();
    CMonitorLogger::instance()->LogDebug(
        "push_current_sections_to_json() writing on the JSON output %lu measurements\n", num_measurements);
}

//------------------------------------------------------------------------------
// Generic routines
//------------------------------------------------------------------------------

void CMonitorOutputFrontend::push_current_sections(bool is_header)
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

size_t CMonitorOutputFrontend::get_current_sample_measurements() const
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

void CMonitorOutputFrontend::pstats()
{
    psection_start("cmonitor_stats");
    plong("section", m_sections);
    plong("subsections", m_subsections);
    plong("string", m_string);
    plong("long", m_long);
    plong("double", m_double);
    plong("hex", m_hex);
    psection_end();
}

void CMonitorOutputFrontend::pheader_start()
{
    // empty for now
}

void CMonitorOutputFrontend::psample_array_start()
{
    if (m_outputJson) {
        push_json_array_start("samples", 1);
    }
}

void CMonitorOutputFrontend::psample_array_end()
{
    if (m_outputJson) {
        push_json_array_end(1);
    }
}

void CMonitorOutputFrontend::psample_start()
{
    // empty for now
}

void CMonitorOutputFrontend::psection_start(const char* section)
{
    m_sections++;

    CMonitorOutputSection sec;
    sec.m_name = section;
    m_current_sections.push_back(sec);

    // when adding new measurements, add them as children of this new section:
    m_current_meas_list = &m_current_sections.back().m_measurements;
}

void CMonitorOutputFrontend::psection_end()
{
    // stop adding measurements to last section:
    m_current_meas_list = nullptr;
}

void CMonitorOutputFrontend::psubsection_start(const char* resource)
{
    m_subsections++;

    CMonitorOutputSubsection sec;
    sec.m_name = resource;
    m_current_sections.back().m_subsections.push_back(sec);

    // when adding new measurements, add them as children of this new subsection:
    m_current_meas_list = &m_current_sections.back().m_subsections.back().m_measurements;
}

void CMonitorOutputFrontend::psubsection_end()
{
    // stop adding measurements to last subsection:
    m_current_meas_list = nullptr;
}

//------------------------------------------------------------------------------
// JSON field/values
//------------------------------------------------------------------------------

void CMonitorOutputFrontend::phex(const char* name, long long value)
{
    m_hex++;
    assert(m_current_meas_list);

    auto fmt_string = fmt::format("hex:{:#08x}", value);
    m_current_meas_list->push_back(CMonitorOutputMeasurement(name, fmt_string.c_str(), true));
}

void CMonitorOutputFrontend::plong(const char* name, long long value)
{
    m_long++;
    assert(m_current_meas_list);

    // according to
    //   https://www.zverovich.net/2020/06/13/fast-int-to-string-revisited.html
    // fmt::format_int is be the fastest way to convert integers
    auto fmt_string = fmt::format_int(value);
    m_current_meas_list->push_back(CMonitorOutputMeasurement(name, fmt_string.c_str(), true));
}

void CMonitorOutputFrontend::pdouble(const char* name, double value)
{
    m_double++;
    assert(m_current_meas_list);

    // with std::to_string() you cannot specify the accuracy (how many decimal digits)
    auto fmt_string = fmt::format("{:.3f}", value);
    m_current_meas_list->push_back(CMonitorOutputMeasurement(name, fmt_string.c_str(), true));
}

void CMonitorOutputFrontend::pstring(const char* name, const char* value)
{
    m_string++;
    assert(m_current_meas_list);

    m_current_meas_list->push_back(CMonitorOutputMeasurement(name, value));
}
