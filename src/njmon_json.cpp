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
long output_char = 0; // number of chars ready to be output in "output" buffer
long level = 0;

const char* saved_section;
const char* saved_resource;
long saved_level = 1;

/* collect stats on the metrics */
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

void NjmonCollectorApp::premove_ending_comma_if_any()
{
    if (output_char >= 2 && output[output_char - 2] == ',') {
        output[output_char - 2] = '\n';
        output_char--;
    }
}

void NjmonCollectorApp::pbuffer_check()
{
    long size;
    if (!output || output_char > (long)(output_size * 0.95)) { /* within 5% of the end */
        size = output_size + (1024 * 1024); /* add another MB */
        output = (char*)realloc((void*)output, size);
        output_size = size;
    }
}

void NjmonCollectorApp::prawc(const char c)
{
    // output a single character:
    output[output_char++] = c;
}

void NjmonCollectorApp::praw(const char* string)
{
    // FIXME: sprintf() is much slower
    output_char += sprintf(&output[output_char], "%s", string);
    /*size_t len = strlen(string);
    strcat(&output[output_char], string);
    output_char += len;*/
}

void NjmonCollectorApp::pstart()
{
    // start new JSON object
    praw("{\n");
}

void NjmonCollectorApp::pfinish()
{
    premove_ending_comma_if_any();
    praw("}\n");
}

void NjmonCollectorApp::psample()
{
    // start JSON data sample
    praw("  {\n");
}

void NjmonCollectorApp::psampleend(bool comma_needed)
{
    premove_ending_comma_if_any();
    if (comma_needed)
        praw("  }\n"); /* end of sample */
    else
        praw("  },\n"); /* end of sample more to come */
}

void NjmonCollectorApp::pindent()
{
    int i;

    for (i = 0; i < saved_level; i++)
        praw("     ");
}

void NjmonCollectorApp::psection(const char* section)
{
    pbuffer_check();
    njmon_sections++;
    saved_section = section;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", section);
    saved_level++;
}

void NjmonCollectorApp::psub(const char* resource)
{
    pbuffer_check();
    njmon_subsections++;
    saved_resource = resource;
    saved_level++;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": {\n", resource);
}

void NjmonCollectorApp::psubend()
{
    saved_resource = NULL;
    premove_ending_comma_if_any();
    pindent();
    praw("},\n");
    saved_level--;
}

void NjmonCollectorApp::psectionend()
{
    saved_section = NULL;
    saved_resource = NULL;
    saved_level--;
    premove_ending_comma_if_any();
    pindent();
    praw("},\n");
}

void NjmonCollectorApp::phex(const char* name, long long value)
{
    pindent();
    njmon_hex++;
    output_char += sprintf(&output[output_char], "\"%s\": \"0x%08llx\",\n", name, value);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::plong(const char* name, long long value)
{
    pindent();
    njmon_long++;
    output_char += sprintf(&output[output_char], "\"%s\": %lld,\n", name, value);
    // LogDebug("plong(%s,%lld) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::pdouble(const char* name, double value)
{
    pindent();
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
    pbuffer_check();
    njmon_string++;
    pindent();
    output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name, value);
    // LogDebug("pstring(%s,%s) count=%ld\n", name, value, output_char);
}

void NjmonCollectorApp::push()
{
    DEBUGLOG_FUNCTION_START();
    pbuffer_check();

    //remote_push();

    if (m_outputJson) {
        if (fputs(output, m_outputJson) < 0) {
            /* if stdout failed there is not must we can do so stop */
            perror("njmon write to output JSON failed, stopping now.");
            exit(99);
        }

        LogDebug("pushed %ld chars in JSON output file", output_char);
    }

    fflush(NULL); /* force I/O output now */

    output[0] = 0;
    output_char = 0;
}
