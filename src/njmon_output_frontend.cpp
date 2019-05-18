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

std::string NjmonOutputFrontend::generate_influxdb_line(
    NjmonMeasurementVector& measurements, const std::string& meas_name, const std::string& ts_nsec)
{
    // format data according to the InfluxDB "line protocol":
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/

    g_logger.LogDebug("generate_influxdb_line() generating measurement: %s\n", meas_name.c_str());

    std::string ret;
    ret.reserve(1024);
    ret += meas_name;

    // FIXME: no tags for now

    // Whitespace I
    ret += " ";

    // Field set
    for (size_t n = 0; n < measurements.size(); n++) {
        auto& m = measurements[n];

        ret += m.m_name.data();
        ret += "=";
        if (m.m_numeric) {
            ret += m.m_value.data();
        } else {
            ret += "\"";
            ret += m.m_value.data();
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

void NjmonOutputFrontend::push_current_sections(bool is_header)
{
    DEBUGLOG_FUNCTION_START();

    size_t num_measurements = get_current_sample_measurements();

    if (m_outputJson) {

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

        g_logger.LogDebug("push_current_sample() writing on the JSON output %lu measurements\n", num_measurements);
    }

    if (m_influxdb_client_conn && !is_header) {

        std::string all_measurements;

        struct timeval tv;
        gettimeofday(&tv, 0);
        uint64_t ts_nsec = ((uint64_t)tv.tv_sec * 1E9) + ((uint64_t)tv.tv_usec * 1E3);

        char ts_nsec_str[64];
        snprintf(ts_nsec_str, sizeof(ts_nsec_str), "%lu", ts_nsec);

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

        g_logger.LogDebug("push_current_sample() pushing to InfluxDB %zu measurements for timestamp: %s\n",
            num_measurements, ts_nsec_str);
        char* influxdb_data = (char*)malloc(all_measurements.size() + 1);
        memcpy(influxdb_data, all_measurements.data(), all_measurements.size());
        post_http_send_line(m_influxdb_client_conn, influxdb_data, all_measurements.size());
    }

    fflush(NULL); /* force I/O output now */

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
    psection_end(); //"njmon_stats");
}

void NjmonOutputFrontend::pheader_start() {}

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

void NjmonOutputFrontend::psample_start() {}

void NjmonOutputFrontend::psection_start(const char* section)
{
#if 1
    m_njmon_sections++;
    NjmonOutputSection sec;
    sec.m_name = section;
    /*
        switch (type) {
        case CONTAINS_MEASUREMENTS:
            m_influxdb_current_measurement = &influxdb_cpp::builder().meas(influxdb_get_meas_name());
            m_influxdb_meas_ready = true;
            break;

        case CONTAINS_SUBSECTIONS:
            m_influxdb_current_measurement = &influxdb_cpp::builder().meas(influxdb_get_meas_name());
            m_influxdb_meas_ready = true;
            break;
        }*/
    m_current_sections.push_back(sec);
    m_current_meas_list = &m_current_sections.back().m_measurements;
#else
    pbuffer_check();
    saved_section = section;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", section);
    saved_level++;

    if (type == CONTAINS_MEASUREMENTS) {
        m_influxdb_current_measurement = &influxdb_cpp::builder().meas(influxdb_get_meas_name());
        m_influxdb_meas_ready = true;
    }
#endif
}

void NjmonOutputFrontend::psection_end()
{
#if 1
    m_current_meas_list = nullptr;
#else
    saved_section = NULL;
    saved_resource = NULL;
    saved_level--;
    premove_ending_comma_if_any();
    pindent();
    praw("},\n");

    if (m_influxdb_meas_ready) {
        // this was a section containing measurements; time to send them out to influxDB:
    }
#endif
}

void NjmonOutputFrontend::psubsection_start(const char* resource)
{
    m_njmon_subsections++;
#if 1
    NjmonOutputSubsection sec;
    sec.m_name = resource;
    m_current_sections.back().m_subsections.push_back(sec);
    m_current_meas_list = &m_current_sections.back().m_subsections.back().m_measurements;
#else
    pbuffer_check();
    saved_resource = resource;
    saved_level++;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", resource);
#endif
}

void NjmonOutputFrontend::psubsection_end()
{
#if 1
    m_current_meas_list = nullptr;
#else
    saved_resource = NULL;
    premove_ending_comma_if_any();
    pindent();
    praw("},\n");
    saved_level--;
#endif
}

//------------------------------------------------------------------------------
// JSON field/values
//------------------------------------------------------------------------------

void NjmonOutputFrontend::phex(const char* name, long long value)
{
    m_njmon_hex++;
    assert(m_current_meas_list);
#if 1
    char buff[128];
    snprintf(buff, sizeof(buff), "0x%08llx", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
#else
    char buff[128];
    snprintf(buff, sizeof(buff), "0x%08llx", value);

    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name, buff);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);

    assert(m_influxdb_meas_ready);
    m_influxdb_current_measurement->field(name, buff);
#endif
}

void NjmonOutputFrontend::plong(const char* name, long long value)
{
    m_njmon_long++;
    assert(m_current_meas_list);
#if 1
    char buff[128];
    snprintf(buff, sizeof(buff), "%lld", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
#else
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": %lld,\n", name, value);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);
#endif
}

void NjmonOutputFrontend::pdouble(const char* name, double value)
{
    m_njmon_double++;
    assert(m_current_meas_list);
#if 1
    char buff[128];
    snprintf(buff, sizeof(buff), "%.3f", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
#else
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": %.3f,\n", name, value);
    // LogDebug("pdouble(%s,%.1f) count=%ld\n", name, value, output_char);
#endif
}

void NjmonOutputFrontend::pstring(const char* name, const char* value)
{
    m_njmon_string++;
    assert(m_current_meas_list);
#if 1
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, value));
#else
    pbuffer_check();
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name, value);
    // LogDebug("pstring(%s,%s) count=%ld\n", name, value, output_char);
#endif
}
