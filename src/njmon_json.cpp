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
#include <unistd.h>

char* output;
long output_size = 0;
long output_char = 0;
long level = 0;

/* collect stats on the metrix */
int njmon_stats = 0;
int njmon_sections = 0;
int njmon_subsections = 0;
int njmon_string = 0;
int njmon_long = 0;
int njmon_double = 0;
int njmon_hex = 0;

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

void NjmonCollectorApp::remove_ending_comma_if_any()
{
    if (output[output_char - 2] == ',') {
        output[output_char - 2] = '\n';
        output_char--;
    }
}

void NjmonCollectorApp::buffer_check()
{
    long size;
    if (!output || output_char > (long)(output_size * 0.95)) { /* within 5% of the end */
        size = output_size + (1024 * 1024); /* add another MB */
        output = (char*)realloc((void*)output, size);
        output_size = size;
    }
}

void NjmonCollectorApp::praw(const char* string) { output_char += sprintf(&output[output_char], "%s", string); }

void NjmonCollectorApp::pstart() { praw("{\n"); }

void NjmonCollectorApp::pfinish()
{
    remove_ending_comma_if_any();
    praw("}\n");
}

void NjmonCollectorApp::psample() { praw("  {\n"); /* start of sample */ }

void NjmonCollectorApp::psampleend(int comma_needed)
{
    remove_ending_comma_if_any();
    if (comma_needed)
        praw("  }\n"); /* end of sample */
    else
        praw("  },\n"); /* end of sample more to come */
}

const char* saved_section;
const char* saved_resource;
long saved_level = 1;

void NjmonCollectorApp::indent()
{
    int i;

    for (i = 0; i < saved_level; i++)
        praw("     ");
}

void NjmonCollectorApp::psection(const char* section)
{
    buffer_check();
    njmon_sections++;
    saved_section = section;
    indent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", section);
    saved_level++;
}

void NjmonCollectorApp::psub(const char* resource)
{
    buffer_check();
    njmon_subsections++;
    saved_resource = resource;
    saved_level++;
    indent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", resource);
}

void NjmonCollectorApp::psubend()
{
    saved_resource = NULL;
    remove_ending_comma_if_any();
    indent();
    praw("},\n");
    saved_level--;
}

void NjmonCollectorApp::psectionend()
{
    saved_section = NULL;
    saved_resource = NULL;
    saved_level--;
    remove_ending_comma_if_any();
    indent();
    praw("},\n");
}

void NjmonCollectorApp::phex(const char* name, long long value)
{
    indent();
    njmon_hex++;
    output_char += sprintf(&output[output_char], "\"%s\": \"0x%08llx\",\n", name, value);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::plong(const char* name, long long value)
{
    indent();
    njmon_long++;
    output_char += sprintf(&output[output_char], "\"%s\": %lld,\n", name, value);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::pdouble(const char* name, double value)
{
    indent();
    njmon_double++;
    output_char += sprintf(&output[output_char], "\"%s\": %.3f,\n", name, value);
    // LogDebug("pdouble(%s,%.1f) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::pstats()
{
    psection("njmon_stats");
    plong("section", njmon_sections);
    plong("subsections", njmon_subsections);
    plong("string", njmon_string);
    plong("long", njmon_long);
    plong("double", njmon_double);
    plong("hex", njmon_hex);
    psectionend(); //"njmon_stats");
}

void NjmonCollectorApp::pstring(const char* name, const char* value)
{
    buffer_check();
    njmon_string++;
    indent();
    output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name, value);
    // LogDebug("pstring(%s,%s) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::push()
{
    DEBUGLOG_FUNCTION_START();
    buffer_check();
    // LogDebug("XXX size=%ld\n", output_char);

    if (m_outputSocketFd) {
        if (write(m_outputSocketFd, output, output_char) < 0) {
            /* if stdout failed there is not must we can do so stop */
            perror("njmon write to output socket failed, stopping now.");
            exit(99);
        }
    }

    if (m_outputJson) {
        if (fputs(output, m_outputJson) < 0) {
            /* if stdout failed there is not must we can do so stop */
            perror("njmon write to output JSON failed, stopping now.");
            exit(99);
        }
    }

    fflush(NULL); /* force I/O output now */

    LogDebug("pushed %ld chars", output_char);
    output[0] = 0;
    output_char = 0;
}
