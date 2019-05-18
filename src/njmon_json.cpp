/*
 * njmon_json.cpp -- collects Linux performance data and generates JSON format data.
 * Developer: Nigel Griffiths.
 * (C) Copyright 2018 Nigel Griffiths

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

#include "njmon.h"
#include <sys/time.h>
#include <unistd.h>

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
/*   p functions to generate JSON output
 *    psection(name) and psectionend()
 *       Adds
 *           "name": {
 *
 *           }
 *
 *    psub(name) and psubend()
 *       similar to psection/psectionend but one level deeper
 *
 *    pstring(name,"abc")
 *    plong(name, 1234)
 *    pdouble(name, 1234.546)
 *    phex(name, hedadecimal number)
 *    praw(name) for other stuff in a raw format
 *       add "name": data,
 *
 *    the JSON is appended to the buffer "output" so
 *        we can remove the trailing "," before we close the entry with a "}"
 *        we can write the whole record in a single write (push()) to help down stream tools
 */

//------------------------------------------------------------------------------
// Low level JSON functions
//------------------------------------------------------------------------------
#if 0
void NjmonOutputFrontend::premove_ending_comma_if_any()
{
    if (output_char >= 2 && output[output_char - 2] == ',') {
        output[output_char - 2] = '\n';
        output_char--;
    }
}

void NjmonOutputFrontend::pbuffer_check()
{
    long size;
    if (!output || output_char > (long)(output_size * 0.95)) { /* within 5% of the end */
        size = output_size + (1024 * 1024); /* add another MB */
        output = (char*)realloc((void*)output, size);
        output_size = size;
    }
}

void NjmonOutputFrontend::praw(const char* string)
{
    // FIXME: sprintf() is much slower
    output_char += sprintf(&output[output_char], "%s", string);
    /*size_t len = strlen(string);
    strcat(&output[output_char], string);
    output_char += len;*/
}

void NjmonOutputFrontend::pindent()
{
    int i;

    for (i = 0; i < saved_level; i++)
        praw("     ");
}
#endif

void NjmonOutputFrontend::pstats()
{
    psection_start("njmon_stats", NjmonOutputFrontend::CONTAINS_MEASUREMENTS);
    plong("section", njmon_sections);
    plong("subsections", njmon_subsections);
    plong("string", njmon_string);
    plong("long", njmon_long);
    plong("double", njmon_double);
    plong("hex", njmon_hex);
    psection_end(); //"njmon_stats");
}

#if 0
void NjmonOutputFrontend::push_influxdb_measurements(
    char** line, int* len, size_t used, NjmonMeasurementVector& measurements, const std::string& meas_name)
{
    used = format_line(line, len, used, // force newline
        INFLUX_MEAS(meas_name.c_str()), // force newline
        INFLUX_TAG("k", "v"), // force newline
        INFLUX_F_STR("dummy", "val"), INFLUX_END);

    for (const auto& m : measurements) {

        // if (m.m_numeric) {

        used = format_line(line, len, used, // force newline
            INFLUX_F_STR(m.m_name, m.m_value), // force newline
            INFLUX_END);
        /*} else {
        }*/
    }
}
#else
std::string NjmonOutputFrontend::generate_influxdb_line(
    NjmonMeasurementVector& measurements, const std::string& meas_name, const std::string& ts_nsec)
{
    // format data according to the InfluxDB "line protocol":
    // see https://docs.influxdata.com/influxdb/v1.7/write_protocols/line_protocol_tutorial/

    printf("Generating measurement: %s\n", meas_name.c_str());

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
#endif

void NjmonOutputFrontend::push_json_measurements(NjmonMeasurementVector& measurements, unsigned int indent)
{
    for (size_t n = 0; n < measurements.size(); n++) {
        auto& m = measurements[n];

        for (size_t i = 0; i < indent; i++)
            fputs("     ", m_outputJson);

        fputs("\"", m_outputJson);
        fputs(m.m_name.data(), m_outputJson);
        if (m.m_numeric) {
            fputs("\": ", m_outputJson);
            fputs(m.m_value.data(), m_outputJson);
            fputs(",\n", m_outputJson);
        } else {
            fputs("\": \"", m_outputJson);
            fputs(m.m_value.data(), m_outputJson);
            fputs("\",\n", m_outputJson);
        }
    }
}

void NjmonOutputFrontend::push_json_object_start(const std::string& str, unsigned int indent)
{
    for (size_t i = 0; i < indent; i++)
        fputs("     ", m_outputJson);
    fputs("\"", m_outputJson);
    fwrite(str.c_str(), 1, str.size(), m_outputJson);
    fputs("\": {\n", m_outputJson);
}

void NjmonOutputFrontend::push_json_object_end(unsigned int indent, bool last)
{
    for (size_t i = 0; i < indent; i++)
        fputs("     ", m_outputJson);
    if (last)
        fputs("}\n", m_outputJson);
    else
        fputs("},\n", m_outputJson);
}

void NjmonOutputFrontend::push_last_sample()
{
    // DEBUGLOG_FUNCTION_START();

    if (m_outputJson) {

        // convert the current sample into JSON format:
        fputs("  {\n", m_outputJson);
        for (size_t i = 0; i < m_current_sample.size(); i++) {
            auto& sec = m_current_sample[i];

            push_json_object_start(sec.m_name, 1);

            if (sec.m_measurements.empty()) {
                for (size_t i = 0; i < sec.m_subsections.size(); i++) {
                    auto& subsec = sec.m_subsections[i];

                    push_json_object_start(subsec.m_name, 2);
                    push_json_measurements(subsec.m_measurements, 3);
                    push_json_object_end(2, i == sec.m_subsections.size() - 1);
                }
            } else {
                push_json_measurements(sec.m_measurements, 2);
            }

            push_json_object_end(1, i == sec.m_subsections.size() - 1);
        }
        fputs("  },\n", m_outputJson);

#if 0
        pbuffer_check();
        if (fputs(output, m_outputJson) < 0) {
            /* if stdout failed there is not must we can do so stop */
            perror("njmon write to output JSON failed, stopping now.");
            exit(99);
        }
#endif
        // LogDebug("pushed %ld chars in JSON output file", output_char);
    }

    if (m_influxdb_client_conn.host) {

        std::string all_measurements;

        struct timeval tv;
        gettimeofday(&tv, 0);
        uint64_t ts_nsec = ((uint64_t)tv.tv_sec * 1E9) + ((uint64_t)tv.tv_usec * 1E3);

        char ts_nsec_str[64];
        snprintf(ts_nsec_str, sizeof(ts_nsec_str), "%lu", ts_nsec);

        size_t ntotal_meas = 0;
        for (size_t i = 0; i < m_current_sample.size(); i++) {
            auto& sec = m_current_sample[i];
            if (sec.m_measurements.empty()) {

                for (size_t i = 0; i < sec.m_subsections.size(); i++) {
                    auto& subsec = sec.m_subsections[i];
                    all_measurements
                        += generate_influxdb_line(subsec.m_measurements, sec.m_name + "_" + subsec.m_name, ts_nsec_str);
                    ntotal_meas += subsec.m_measurements.size();

                    if (i < sec.m_subsections.size() - 1)
                        all_measurements += "\n";
                }

            } else {
                all_measurements += generate_influxdb_line(sec.m_measurements, sec.m_name, ts_nsec_str);
                ntotal_meas += sec.m_measurements.size();
            }

            if (i < m_current_sample.size() - 1)
                all_measurements += "\n";
        }

        printf("Pushing to InfluxDB a total of %zu measurements for this timestamp: %s\n", ntotal_meas, ts_nsec_str);
        char* influxdb_data = (char*)malloc(all_measurements.size() + 1);
        memcpy(influxdb_data, all_measurements.data(), all_measurements.size());
        post_http_send_line(&m_influxdb_client_conn, influxdb_data, all_measurements.size());
    }

    fflush(NULL); /* force I/O output now */

    // output[0] = 0;
    // output_char = 0;
}

//------------------------------------------------------------------------------
// JSON objects
//------------------------------------------------------------------------------

void NjmonOutputFrontend::psample_start()
{
    // start JSON data sample
    // praw("  {\n");
    m_current_sample.clear();
}

void NjmonOutputFrontend::psample_end(bool comma_needed)
{
#if 1
#else
    premove_ending_comma_if_any();
    if (comma_needed)
        praw("  }\n"); /* end of sample */
    else
        praw("  },\n"); /* end of sample more to come */
#endif
}

void NjmonOutputFrontend::psection_start(const char* section, SectionType type)
{
#if 1
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
    m_current_sample.push_back(sec);
    m_current_meas_list = &m_current_sample.back().m_measurements;
#else
    pbuffer_check();
    njmon_sections++;
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
#if 1
    NjmonOutputSubsection sec;
    sec.m_name = resource;
    m_current_sample.back().m_subsections.push_back(sec);
    m_current_meas_list = &m_current_sample.back().m_subsections.back().m_measurements;
#else
    pbuffer_check();
    njmon_subsections++;
    saved_resource = resource;
    saved_level++;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", resource);
#endif
}

void NjmonOutputFrontend::psubsection_end()
{
#if 1

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
#if 1
    char buff[128];
    snprintf(buff, sizeof(buff), "0x%08llx", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
#else
    char buff[128];
    snprintf(buff, sizeof(buff), "0x%08llx", value);

    pindent();
    njmon_hex++;
    output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name, buff);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);

    assert(m_influxdb_meas_ready);
    m_influxdb_current_measurement->field(name, buff);
#endif
}

void NjmonOutputFrontend::plong(const char* name, long long value)
{
#if 1
    char buff[128];
    snprintf(buff, sizeof(buff), "%lld", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
#else
    pindent();
    njmon_long++;
    output_char += sprintf(&output[output_char], "\"%s\": %lld,\n", name, value);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);
#endif
}

void NjmonOutputFrontend::pdouble(const char* name, double value)
{
#if 1
    char buff[128];
    snprintf(buff, sizeof(buff), "%.3f", value);
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, buff, true));
#else
    pindent();
    njmon_double++;
    output_char += sprintf(&output[output_char], "\"%s\": %.3f,\n", name, value);
    // LogDebug("pdouble(%s,%.1f) count=%ld\n", name, value, output_char);
#endif
}

void NjmonOutputFrontend::pstring(const char* name, const char* value)
{
#if 1
    m_current_meas_list->push_back(NjmonOutputMeasurement(name, value));
#else
    pbuffer_check();
    njmon_string++;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name, value);
    // LogDebug("pstring(%s,%s) count=%ld\n", name, value, output_char);
#endif
}
