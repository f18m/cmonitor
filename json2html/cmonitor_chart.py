#!/usr/bin/python3 

#
# cmonitor_chart.py
# Originally based on the "njmonchart_aix_v7.py" from Nigel project: http://nmon.sourceforge.net/
#
# Author: Francesco Montorsi
# Created: April 2019
#

import sys
import json
import gzip
import datetime
import zlib
import binascii
import textwrap

# =======================================================================================================
# CONSTANTS
# =======================================================================================================

GRAPH_TYPE_BAREMETAL = 1
GRAPH_TYPE_CGROUP = 2

SAVE_DEFLATED_JS_DATATABLES = True
JS_INDENT_SIZE = 2


# =======================================================================================================
# GoogleChartsTable
# This is the table of X;Y1;Y2;...;YN data points for a GoogleCharts graph
# =======================================================================================================
    
class GoogleChartsTable(object):

    def __init__(self, column_names):
        self.column_names = column_names  # must be a LIST of strings
        self.rows = []  # list of lists with values

    def ISOdatetimeToJSDate(self, date):
        ''' convert ISO datetime strings like 
              "2017-08-21T20:12:30" 
            to strings like:
              "Date(2019,4,04,01,16,25)"
            which are the datetime representation suitable for JS GoogleCharts
         '''
        dateAsPythonObj = datetime.datetime.strptime(date, "%Y-%m-%dT%H:%M:%S")
        return "Date(%d,%d,%d,%d,%d,%d)" % (dateAsPythonObj.year, dateAsPythonObj.month, dateAsPythonObj.day, dateAsPythonObj.hour, dateAsPythonObj.minute, dateAsPythonObj.second)

    def addRow(self, row_data_list):
        assert(len(row_data_list) == len(self.column_names))
        
        # convert first column to a GoogleCharts-compatible datetime:
        row_data_list[0] = self.ISOdatetimeToJSDate(row_data_list[0])
        self.rows.append(row_data_list)

    def getRow(self, index):
        return self.rows[index]
    
    def getListColumnNames(self):
        return self.column_names
    
    def getNumDataSeries(self):
        # assuming first column is the timestamp, the number of "data series"
        # present in this table is all remaining columns
        return len(self.column_names)-1
    
    def writeTo(self, file):
        for r in self.rows:
            # assume first column is always the timestamp:
            row_text = "['Date(%s)'," % r[0]
            row_text += ','.join(str(x) for x in r[1:])
            row_text += '],\n'
            file.write(row_text)

    def toJSONForJS(self):
        ret = '[[' # start 2D JSON array
        
        # convert 1st column:
        assert(self.column_names[0] == 'Timestamp')
        ret += '{"type":"datetime","label":"Datetime"},'
        
        # convert all other columns:
        for colName in self.column_names[1:]:
            ret += '"' + colName + '",'
        ret = ret[:-1]
        
        # separe first line; start conversion of actual table data:
        ret += '],'
        
        data = json.dumps(self.rows, separators=(',', ':'))
        data = data[1:]

        return ret + data
    
    def toDeflatedJSONBase64Encoded(self):
        ''' Returns this table in JSON format (for JS), deflated using zlib, and represented as a Base64-encoded ASCII string '''
        json_string = self.toJSONForJS()
        json_compressed_bytearray = zlib.compress(json_string.encode(), 9)
        
        ret = str(binascii.b2a_base64(json_compressed_bytearray))
        return ret[1:]

    def toGoogleChartTable(self, graphName):
        ''' Writes in the given file the JavaScript GoogleCharts object representing this table '''
        ret_string = ""
        if SAVE_DEFLATED_JS_DATATABLES:
            # to reduce the HTML size save the deflated, serialized JSON of the 2D JS array:
            ret_string += "var deflated_data_base64_%s = %s;\n" % (graphName, self.toDeflatedJSONBase64Encoded())
            
            # then convert it base64 -> JS binary string
            ret_string += "var deflated_data_binary_%s = window.atob(deflated_data_base64_%s);\n" % (graphName, graphName)
            
            # now inflate it in the browser using "pako" library (https://github.com/nodeca/pako)
            ret_string += "var inflated_data_%s = JSON.parse(pako.inflate(deflated_data_binary_%s, { to: 'string' }));\n" % (graphName, graphName)
        else:
            ret_string += "var inflated_data_%s = %s;\n" % (graphName, self.toJSONForJS())
        
        # finally create the GoogleCharts table from it:
        ret_string += "var data_%s = google.visualization.arrayToDataTable(inflated_data_%s);\n" % (graphName, graphName)
        return ret_string


# =======================================================================================================
# GoogleChartsGraph
# This is a simple object that can generate JavaScript for GoogleChart drawing 
# =======================================================================================================
    
class GoogleChartsGraph:

    def __init__(self, 
                 button_label="", 
                 graph_type=GRAPH_TYPE_BAREMETAL, 
                 graph_title="", 
                 stack_state=False, 
                 y_axis_title="", 
                 series_for_2nd_yaxis=[],
                 data=[]):
        self.button_label = button_label
        self.type = graph_type  # one of GRAPH_TYPE_BAREMETAL or GRAPH_TYPE_CGROUP
        self.graph_title = graph_title
        self.stack_state = stack_state
        self.y_axis_title = y_axis_title
        self.series_for_2nd_yaxis = series_for_2nd_yaxis
        self.data_table = data
        self.graph_title += (', STACKED graph' if self.stack_state else '')

        # generate new JS name for this graph
        global g_num_generated_charts 
        self.js_name = 'graph' + str(g_num_generated_charts)
        g_num_generated_charts += 1

        
    def genGoogleChartJSDrawFunction(self, ):
        ''' After the JavaScript line graph data is output, the data is terminated and the graph options set'''
        global next_graph_need_stacking
        
        def __internalWriteAxis(series_indexes, target_axis_index):
            ret = ""
            for i, idx in enumerate(series_indexes, start=0):
                ret += '   %d: {targetAxisIndex:%d}' % (idx,target_axis_index)
                #print("i=%d, idx=%d, target_axis_index=%d" % (i,idx,target_axis_index))
                if i < len(series_indexes):
                    ret += ',\n'
                else:
                    ret += '\n'
            return ret
        
        ret_string = ""
        ret_string += 'var options_%s = {\n' % (self.js_name)
        ret_string += '  chartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n'
        ret_string += '  title: "%s",\n' % (self.graph_title)
        ret_string += '  focusTarget: "category",\n'
        ret_string += '  hAxis: { gridlines: { color: "lightgrey", count: 30 } },\n'
        if len(self.series_for_2nd_yaxis)>0:
            # compute series that use 1st Y axis:
            num_series=self.data_table.getNumDataSeries()
            series_for_1st_yaxis = range(0,num_series)
            series_for_1st_yaxis = [item for item in series_for_1st_yaxis if item not in self.series_for_2nd_yaxis]
            #print("series_for_1st_yaxis: %s" % ','.join(str(x) for x in series_for_1st_yaxis))
            #print("self.series_for_2nd_yaxis: %s" % ','.join(str(x) for x in self.series_for_2nd_yaxis))
    
            # assign data series to the 2 Y axes:
            ret_string += '  series: {\n'
            ret_string += __internalWriteAxis(series_for_1st_yaxis, 0)
            ret_string += __internalWriteAxis(self.series_for_2nd_yaxis, 1)
            ret_string += '  },\n'
            
            # allocate 2 Y axes:
            assert(len(self.y_axis_title) == 2)
            ret_string += '  vAxes: {\n'
            ret_string += '    0: { title: "%s" },\n' % str(self.y_axis_title[0])
            ret_string += '    1: { title: "%s" }\n' % str(self.y_axis_title[1])
            ret_string += '  },\n'
        else:
            ret_string += '  vAxis: { title: "%s", gridlines: { color: "lightgrey", count: 11 } },\n' % str(self.y_axis_title)
        ret_string += '  explorer: { actions: ["dragToZoom", "rightClickToReset"], axis: "horizontal", keepInBounds: true, maxZoomIn: 20.0 },\n'
        
        # graph stacking
        next_graph_need_stacking = self.stack_state
        if next_graph_need_stacking:
            ret_string += '  isStacked:  1\n'
            next_graph_need_stacking = 0
        else:
            ret_string += '  isStacked:  0\n'
            
        ret_string += '};\n'  # end of "options_%s" variable
        ret_string += '\n'
        ret_string += 'if (g_chart && g_chart.clearChart)\n'
        ret_string += '  g_chart.clearChart();\n'
        ret_string += 'g_chart = new google.visualization.AreaChart(document.getElementById("chart_master"));\n'
        ret_string += 'g_chart.draw(data_%s, options_%s);\n' % (self.js_name, self.js_name)
        return ret_string
    
#     def bubbleit(self,web, column_names, data, title, button_label, graph_type):
#         global g_graphs
#         
#         # declare JS variables:
#         nchart_start_js_bubble_graph(web, column_names)
#         nchart_write_js_graph_data(web, data)
#         nchart_end_js_bubble_graph(web, title)
        
    def toGoogleChartJS(self):
        global next_graph_need_stacking
        
        # generate the JS
        js_code_inner = self.data_table.toGoogleChartTable(self.js_name)
        js_code_inner += self.genGoogleChartJSDrawFunction()

        js_code = 'function draw_%s() {\n' % (self.js_name)
        js_code += textwrap.indent(js_code_inner, ' ' * JS_INDENT_SIZE)
        js_code += '}\n' # end of draw_%s function
        js_code += '\n'

        return js_code
        

# =======================================================================================================
# GLOBALs
# =======================================================================================================

g_graphs = []  # global list of GoogleChartsGraph class instances
g_num_generated_charts = 1


# =======================================================================================================
# nchart_* routines to actually produce HTML+JavaScript
# =======================================================================================================

def nchart_start_js(file, title):
    ''' Write the head of the HTML webpage and start the JS section '''
    file.write('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">\n')
    file.write('<html xmlns="http://www.w3.org/1999/xhtml">' + '\n')
    file.write('<head>' + '\n')
    file.write('  <title>' + title + '</title>\n')
    file.write('  <style>\n')
    file.write('     html,body {height:85%;}\n')
    file.write('     h3 {margin: 0px;}\n')
    file.write('     ul {margin: 0 0 0 0;padding-left: 20px;}\n')
    file.write('     button { margin-bottom: 3px; }\n')
    file.write('     #chart_master {width:100%; height:85%;}\n')
    file.write('     #bottom_div {float:left; border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px;}\n')
    file.write('     #bottom_div h3 {font-size: medium;}\n')
    file.write('     #bottom_div li {font-size: smaller;}\n')
    file.write('     #bottom_table_val {font-family: monospace;}\n')
    file.write('     #button_table { border-collapse: collapse; }\n')
    file.write('     #button_table_col {border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px;}\n')
    file.write('  </style>\n')
    file.write('  <script type="text/javascript" src="https://cdn.jsdelivr.net/npm/pako@1.0.10/dist/pako.min.js"></script>\n')
    file.write('  <script type="text/javascript" src="https://www.google.com/jsapi"></script>\n')
    file.write('  <script type="text/javascript">\n')
    file.write('/* Load GoogleCharts: */\n')
    file.write('google.load("visualization", "1.1", {packages:["corechart"]});\n')
    file.write('google.setOnLoadCallback(setup_button_click_handlers);\n')
    file.write('\n')
    file.write('/* The global chart object: */\n')
    file.write('var g_chart = null;\n')
    file.write('\n')
    file.write('/* The global window showing the configuration of all collected data: */\n')
    file.write('var g_configWindow = null;\n')
    file.write('\n')
    # at this point we will generate all helper JS functions

def nchart_end_js(file, config):
    ''' Finish the JS portion and HTML head tag '''
    
    global g_graphs
    
    # add all event listeners for button clicks:
    file.write('function setup_button_click_handlers() {\n')
    for num, graph in enumerate(g_graphs, start=1):
        file.write('  document.getElementById("btn_draw_%s").addEventListener("click", draw_%s);\n' % (graph.js_name, graph.js_name))
    file.write('  document.getElementById("btn_show_config").addEventListener("click", show_config_window);\n')
    file.write('}\n')
    
    file.write(config)
    file.write('  </script>\n')
    file.write('</head>\n')
    
    
# def nchart_start_js_bubble_graph(file, columnnames):
#     ''' Before the graph data with datetime + multiple columns of data '''
#     global g_num_generated_charts 
#     file.write('  var data_' + str(g_num_generated_charts) + ' = google.visualization.arrayToDataTable([\n')
#     file.write("[" + columnnames + "]\n")
#     
# def nchart_end_js_bubble_graph(file, graphtitle):
#     ''' After the JavaScript bubble graph data is output, the data is terminated and the bubble graph options set'''
#     global g_num_generated_charts
#     file.write('  ]);\n')
#     file.write('  var options_' + str(g_num_generated_charts) + ' = {\n')
#     file.write('    chartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n')
#     file.write('    title: "' + graphtitle + '",\n')
#     file.write('    hAxis: { title:"CPU seconds in Total"},\n')
#     file.write('    vAxis: { title:"Character I/O in Total"},\n')
#     file.write('    sizeAxis: {maxSize: 200},\n')
#     file.write('    bubble: {textStyle: {fontSize: 15}}\n')
#     file.write('  };\n')
#     file.write('  document.getElementById("draw_' + str(g_num_generated_charts) + '").addEventListener("click", function() {\n')
#     file.write('  if (chart && chart.clearChart)\n')
#     file.write('    chart.clearChart();\n')
#     file.write('  chart = new google.visualization.BubbleChart(document.getElementById("chart_master"));\n')
#     file.write('  chart.draw(data_' + str(g_num_generated_charts) + ', options_' + str(g_num_generated_charts) + ');\n')
#     file.write('  });\n')
#     g_num_generated_charts += 1
    

def nchart_start_html_body(file, hostname):
    global g_graphs
    file.write('<body bgcolor="#EEEEFF">\n')
    file.write('  <h1>Monitoring data for hostname: ' + hostname + '</h1>\n')
    file.write('  <div id="button_div">\n')
    file.write('  <table id="button_table">\n')
    
    # Table header row
    file.write('  <tr><td id="button_table_col"></td><td id="button_table_col"><b>CGroup</b></td><td id="button_table_col"><b>Baremetal</b> (Data collected from /proc)</td></tr>\n')
    
    # Datarow
    file.write('  <tr>\n')
    file.write('  <td id="button_table_col">\n')
    file.write('    <button id="btn_show_config"><b>Configuration</b></button><br/>\n')
    file.write('  </td><td id="button_table_col">\n')

    def write_buttons_for_graph_type(type):
        # find in all graphs registered so far all those related to the CGROUP
        for num, graph in enumerate(g_graphs, start=1):
            if graph.type == type:
                if graph.button_label.startswith('CPU') or graph.button_label.endswith('CPUs'):
                    colour = 'red'
                elif graph.button_label.startswith('Memory'):
                    colour = 'darkorange'
                elif graph.button_label.startswith('Network'):
                    colour = 'darkblue'
                elif graph.button_label.startswith('Disk'):
                    colour = 'darkgreen'
                else:
                    colour = 'black'
                file.write('    <button id="btn_draw_' + graph.js_name + '" style="color:' + colour + '"><b>' + graph.button_label + '</b></button>\n')

    write_buttons_for_graph_type(GRAPH_TYPE_CGROUP)
    file.write('      </td><td id="button_table_col">\n')
    write_buttons_for_graph_type(GRAPH_TYPE_BAREMETAL)
        
    file.write('  </td></tr>\n')
    file.write('  </table>\n')
    file.write('  </div>\n')
    file.write('  <p></p>\n')
    file.write('  <div id="chart_master"></div>\n')

    
def nchart_append_html_table(file, name, table_entries):
    file.write('  <div id="bottom_div">\n')
    file.write('    <h3>' + name + '</h3>\n')
    file.write('    <table>\n')
    file.write('    <tr><td><ul>\n')
    for i, entry in enumerate(table_entries, start=1):
        file.write("      <li>" + entry[0] + " <span id='bottom_table_val'>" + entry[1]+ "</span></li>\n")
        if (i % 4) == 0:
            file.write("      </ul></td><td><ul>\n")
    file.write('    </ul></td></tr>\n')
    file.write('    </table>\n')
    file.write('  </div>\n')


def nchart_end_html_body(file):
    file.write('</body>\n')
    file.write('</html>\n')



# =======================================================================================================
# generate_* routines
# =======================================================================================================


def generate_config_js(jheader):

    # ----- add config box 
    def configdump(section, displayName):
        #newstr = '<h3>' + displayName + '</h3>\\\n'
        newstr = "<tr><td colspan='2' id='sectioncol'>" + displayName + "</td></tr>\\\n"
        config_dict = jheader[section]
        for label in config_dict:
            newstr += "    <tr>\\\n"
            newstr += "    <td id='configkey'>%s</td><td id='configval'>%s</td>\\\n" % (label.capitalize().replace("_", " "), str(config_dict[label]))
            newstr += "    </tr>\\\n"
        return newstr
    
    def sizeof_fmt(num, suffix='B'):
        for unit in ['', 'Ki', 'Mi', 'Gi', 'Ti', 'Pi', 'Ei', 'Zi']:
            if abs(num) < 1024.0:
                return "%3.1f%s%s" % (num, unit, suffix)
            num /= 1024.0
        return "%.1f%s%s" % (num, 'Yi', suffix)
    
    # provide some human-readable config files:
    if 'cgroup_config' in jheader:
        avail_cpus = jheader['cgroup_config']['cpus'].split(',')
        jheader['cgroup_config']['num_allowed_cpus'] = len(avail_cpus)
        jheader['cgroup_config']['memory_limit_bytes'] = sizeof_fmt(int(jheader['cgroup_config']['memory_limit_bytes']))
        jheader['cgroup_config']['cpus'] = jheader['cgroup_config']['cpus'].replace(',', ', ')
        
    if 'cmonitor' in jheader:
        if jheader['cmonitor']['sample_num'] == 0:
            jheader['cmonitor']['sample_num'] = "Infinite"
          
    config_str = ""
    config_str += '\nfunction show_config_window() {\n'
    config_str += '    if (g_configWindow) g_configWindow.close();\n'
    config_str += '    g_configWindow = window.open("", "MsgWindow", "width=1024, height=800, toolbar=no");\n'
    config_str += '    g_configWindow.document.write("\\\n'
    config_str += '    <html><head>\\\n'
    config_str += '      <title>Configuration</title>\\\n'
    config_str += '      <style>\\\n'
    config_str += '        table { padding-left: 2ex; }\\\n'
    config_str += '        #sectioncol {font-weight: bold; padding: 1ex; font-size: large;background-color: lightsteelblue;}\\\n'
    config_str += '        #configkey {font-weight: bold;}\\\n'
    config_str += '        #configval {font-family: monospace;}\\\n'
    config_str += '      </style>\\\n'
    config_str += '    </head><body>\\\n'
    config_str += '      <h2>Monitored System Summary</h2>\\\n'
    config_str += '      <table>\\\n'
    config_str += configdump("identity", "Server Identity")
    config_str += configdump("os_release", "Operating System Release")
    config_str += configdump("proc_version", "Linux Kernel Version")
    if 'cgroup_config' in jheader:
        config_str += configdump("cgroup_config", "Linux Control Group (CGroup) Configuration")
    config_str += configdump("lscpu", "CPU Overview")
    #config_str += configdump("cpuinfo", "CPU Core Details")
    config_str += '      </table>\\\n'
    config_str += '      <h2>Monitoring Summary</h2>\\\n'
    config_str += '      <table>\\\n'
    config_str += configdump("cmonitor", "Performance Stats Collector Configuration")
    config_str += '      </table>\\\n'
    config_str += '    </body></html>\\\n'
    config_str += '");\n}\n\n'
    return config_str


# def generate_top20_procs(jdata):
#     # - - - Top 20 Processes
#     # Check if there are process stats in this .json file as they are optional
#     start_processes = "unknown"
#     end_processes = "unknown"
#     try:
#         start_processes = len(jdata[0]["processes"])
#         end_processes = len(jdata[-1]["processes"])
#         process_data_found = True
#     except:
#         process_data_found = False
#     
#     if process_data_found:
#         top = {}  # start with empty dictionary
#         for sam in jdata:
#             for process in sam["processes"]:
#                 entry = sam['processes'][process]
#                 if entry['ucpu_time'] != 0.0 and entry['scpu_time'] != 0.0:
#                     # print("%20s pid=%d ucpu=%0.3f scpu=%0.3f mem=%0.3f io=%0.3f"%(entry['name'],entry['pid'],
#                     #        entry['ucpu_time'],entry['scpu_time'],
#                     #        entry['real_mem_data']+entry['real_mem_text'],
#                     #        entry['inBytes']+entry['outBytes']))
#                     try:  # update the current entry
#                         top[entry['name']]["cpu"] += entry['ucpu_time'] + entry['scpu_time']
#                         top[entry['name']]["io"] += entry['inBytes'] + entry['inBytes']
#                         top[entry['name']]["mem"] += entry['real_mem_data'] + entry['real_mem_text']
#                     except:  # no current entry so add one
#                         top.update({entry['name']: { "cpu": entry['ucpu_time'] + entry['scpu_time'],
#                                   "io": entry['inBytes'] + entry['outBytes'],
#                                   "mem": entry['real_mem_data'] + entry['real_mem_text']} })
# 
#         def sort_key(d):
#             return top[d]['cpu']
#     
#         topprocs = ""
#         tops = []
#         for i, proc in enumerate(sorted(top, key=sort_key, reverse=True)):
#             p = top[proc]
#             # print("%20s cpu=%0.3f io=%0.3f mem=%0.3f"%(proc, p['cpu'],p['io'],p['mem']))
#             topprocs += ",['%s',%.1f,%.1f,'%s',%.1f]\n" % (proc, p['cpu'], p['io'], proc, p['mem'])
#             tops.append(proc)
#             if i >= 20:  # Only graph the top 20
#                 break
#     
#         topprocs_title = "'Command', 'CPU seconds', 'CharIO', 'Type', 'Memory KB'"
#     
#         top_header = ""
#         for proc in tops:
#             top_header += "'" + proc + "',"
#         top_header = top_header[:-1]
#     
#         top_data = ""
#         for sam in jdata:
#             top_data += ",['Date(%s)'" % (googledate(sam['timestamp']['datetime']))
#             for item in tops:
#                 bytes = 0
#                 for proc in sam['processes']:
#                     p = sam['processes'][proc]
#                     if p['name'] == item:
#                         bytes += p['ucpu_time'] + p['scpu_time']
#                 top_data += ", %.1f" % (bytes)
#             top_data += "]\n"
#         # print(top_header)
#         # print(top_data)
#     return (start_processes, end_processes, process_data_found)


# def generate_top20_disks(jdata):
#     tdisk = {}  # start with empty dictionary
#     for sam in jdata:
#             for disk in sam["disks"]:
#                 entry = sam['disks'][disk]
#                 bytes = entry['rkb'] + entry['wkb']
#                 if bytes != 0:
#                     # print("disk=%s total bytes=%.1f"%(disk,bytes))
#                     try:  # update the current entry
#                         tdisk[entry[disk]] += bytes
#                     except:  # no current entry so add one
#                         tdisk.update({disk: bytes})
# 
#     def sort_dkey(d):
#             return tdisk[d]
#     
#     topdisks = []
#     for i, disk in enumerate(sorted(tdisk, key=sort_dkey, reverse=True)):
#         d = tdisk[disk]
#         # print("disk=%s total bytes=%.1f"%(disk,bytes))
#         topdisks.append(disk)
#         if i >= 20:  # Only graph the top 20
#             break
#     # print(topdisks)
#     
#     td_header = ""
#     for disk in topdisks:
#         td_header += "'" + disk + "',"
#     td_header = td_header[:-1]
#     
#     td_data = ""
#     for sam in jdata:
#         td_data += ",['Date(%s)'" % (googledate(sam['timestamp']['datetime']))
#         for item in topdisks:
#             bytes = sam['disks'][item]['rkb'] + sam['disks'][item]['wkb']
#             td_data += ", %.1f" % (bytes)
#         td_data += "]\n"
#     # print(td_header)
#     # print(td_data)
#     return (tdisk, td_header, td_data)


def generate_disks_io(web, jdata, hostname):
    # if network traffic data was not collected, just return:
    if 'disks' not in jdata[0]:
        return
    
    all_disks = jdata[0]["disks"].keys()
    if len(all_disks) == 0:
        return
    
    diskcols = ['Timestamp']
    for device in all_disks:
        #diskcols.append(str(device) + " Disk Time")
        #diskcols.append(str(device) + " Reads")
        #diskcols.append(str(device) + " Writes")
        diskcols.append(str(device) + " Read MB")
        diskcols.append(str(device) + " Write MB")

    # convert from kB to MB
    divider = 1000
    
    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTable() object
    #
    
    disk_table = GoogleChartsTable(diskcols)
    for i, s in enumerate(jdata):
        if i == 0:
            continue

        row = []
        row.append(s['timestamp']['datetime'])
        for device in all_disks:
            #row.append(s["disks"][device]["time"])
            #row.append(s["disks"][device]["reads"])
            #row.append(s["disks"][device]["writes"])
            row.append(s["disks"][device]["rkb"]/divider)
            row.append(-s["disks"][device]["wkb"]/divider)
        disk_table.addRow(row)

    g = GoogleChartsGraph(button_label='Disk I/O', 
                          graph_type=GRAPH_TYPE_BAREMETAL,
                          graph_title="Disk I/O (from baremetal stats)",
                          y_axis_title="MB",
                          data=disk_table)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals

# def generate_filesystems(web, jdata):
#     global g_graphs
#     fsstr = ""
#     for fs in jdata[0]["filesystems"].keys():
#         fsstr = fsstr + "'" + fs + "',"
#     fsstr = fsstr[:-1]
#     nchart_start_js_line_graph(web, fsstr)
#     for i, s in enumerate(jdata):
#         web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
#         for fs in s["filesystems"].keys():
#             web.write(", %.1f" % (s["filesystems"][fs]["fs_full_percent"]))
#         web.write("]\n")
#     g = GoogleChartsGraph( 'File Systems Used percent')
#     g_graphs.append("File Systems")
#     return web


def generate_network_traffic(web, jdata, hostname):
    # if network traffic data was not collected, just return:
    if 'network_interfaces' not in jdata[0]:
        return
    
    all_netdevices = jdata[0]["network_interfaces"].keys()
    if len(all_netdevices) == 0:
        return
    
    netcols = ['Timestamp']
    for device in all_netdevices:
        netcols.append(str(device) + "+in")
        netcols.append(str(device) + "-out")

    # convert from bytes to MB
    divider = 1000 * 1000

    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTable() object
    #
    
    
    # MB/sec
    
    net_table = GoogleChartsTable(netcols)
    for i, s in enumerate(jdata):
        if i == 0:
            continue

        row = [ s['timestamp']['datetime'] ]
        for device in all_netdevices:
            try:
                row.append(+s["network_interfaces"][device]["ibytes"]/divider)
                row.append(-s["network_interfaces"][device]["obytes"]/divider)
            except KeyError:
                print("Missing key '%s' while parsing sample %d" % (device, i))
                row.append(0)
                row.append(0)
        net_table.addRow(row)

    g = GoogleChartsGraph(
            graph_title='Network Traffic in MB/s for ' + hostname + " (from baremetal stats)",
            button_label='Network Traffic (MB/s)',
            y_axis_title="MB/s",
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False,
            data=net_table)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals
    
            
    # PPS
    
    net_table = GoogleChartsTable(netcols)
    for i, s in enumerate(jdata):
        if i == 0:
            continue

        row = [ s['timestamp']['datetime'] ]
        for device in all_netdevices:
            try:
                row.append(+s["network_interfaces"][device]["ipackets"])
                row.append(-s["network_interfaces"][device]["opackets"])
            except KeyError:
                print("Missing key '%s' while parsing sample %d" % (device, i))
                row.append(0)
                row.append(0)
        net_table.addRow(row)

    g = GoogleChartsGraph(
            graph_title='Network Traffic in PPS for ' + hostname + " (from baremetal stats)",
            button_label='Network Traffic (PPS)',
            y_axis_title="PPS",
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False,
            data=net_table)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals
    
    return web


def generate_baremetal_cpus(web, jdata, logical_cpus_indexes, hostname):
    # if baremetal CPU data was not collected, just return:
    if 'stat' not in jdata[0]:
        return

    # prepare empty tables
    baremetal_cpu_stats = {}
    for c in logical_cpus_indexes:
        baremetal_cpu_stats[c] = GoogleChartsTable(['Timestamp', 'User', 'Nice', 'System', 'Idle', 'I/O wait', 'Hard IRQ', 'Soft IRQ', 'Steal'])
        
    all_cpus_table = GoogleChartsTable(['Timestamp'] + [('CPU' + str(x)) for x in logical_cpus_indexes])
    
    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTable() object
    #

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample

        ts = s['timestamp']['datetime']
        all_cpus_row = [ ts ]
        for c in logical_cpus_indexes:
            cpu_stats = s['stat']['cpu' + str(c)]
            cpu_total = cpu_stats['user'] + cpu_stats['nice'] + cpu_stats['sys'] + \
                        cpu_stats['iowait'] + cpu_stats['hardirq'] + cpu_stats['softirq'] + cpu_stats['steal']
            baremetal_cpu_stats[c].addRow([ \
                    ts,
                    cpu_stats['user'],
                    cpu_stats['nice'],
                    cpu_stats['sys'],
                    cpu_stats['idle'],
                    cpu_stats['iowait'],
                    cpu_stats['hardirq'],
                    cpu_stats['softirq'],
                    cpu_stats['steal']
                ])
            all_cpus_row.append(cpu_total)
        
        all_cpus_table.addRow(all_cpus_row)
    
    # Produce the javascript:
    for c in logical_cpus_indexes:
        g = GoogleChartsGraph(
                data=baremetal_cpu_stats[c],  # Data
                graph_title='Logical CPU ' + str(c) + " (from baremetal stats)",
                button_label="CPU" + str(c),
                y_axis_title="Time (%)",
                graph_type=GRAPH_TYPE_BAREMETAL,
                stack_state=True)
        web.write(g.toGoogleChartJS())
        g_graphs.append(g)    # register this graph into globals

    # Also produce the "all CPUs" graph
    g = GoogleChartsGraph(
            data=all_cpus_table,  # Data
            graph_title="All logical CPUs allowed in cmonitor_collector CGroup (from baremetal stats)",
            button_label="All CPUs",
            y_axis_title="Time (%)",
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals
    

def generate_cgroup_cpus(web, jdata, logical_cpus_indexes, hostname):
    if 'cgroup_cpuacct_stats' not in jdata[0]:
        return  # cgroup mode not enabled at collection time!
        
    # prepare empty tables
    cpu_stats_table = {}
    for c in logical_cpus_indexes:
        cpu_stats_table[c] = GoogleChartsTable(['Timestamp', 'User', 'System'])
        
    all_cpus_table = GoogleChartsTable(['Timestamp'] + [('CPU' + str(x)) for x in logical_cpus_indexes])
        
    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTable() object
    #

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        ts = s['timestamp']['datetime']
        all_cpus_row = [ ts ]
        for c in logical_cpus_indexes:
            # get data:
            cpu_stats = s['cgroup_cpuacct_stats']['cpu' + str(c)]
            if 'sys' in cpu_stats:
                cpu_sys = cpu_stats['sys']
            else:
                cpu_sys = 0
            cpu_total = cpu_stats['user'] + cpu_sys

            # append data:
            cpu_stats_table[c].addRow([ts, cpu_stats['user'], cpu_sys])
            all_cpus_row.append(cpu_total)
        
        all_cpus_table.addRow(all_cpus_row)

    # Produce 1 graph for each CPU:
    for c in logical_cpus_indexes:
        g = GoogleChartsGraph(
                data=cpu_stats_table[c],  # Data
                graph_title='Logical CPU ' + str(c) + " (from CGroup stats)",
                button_label="CPU" + str(c),
                y_axis_title="Time (%)",
                graph_type=GRAPH_TYPE_CGROUP,
                stack_state=True)
        web.write(g.toGoogleChartJS())
        g_graphs.append(g)    # register this graph into globals

    # Also produce the "all CPUs" graph
    g = GoogleChartsGraph(
            data=all_cpus_table,  # Data
            graph_title="All logical CPUs assigned to cmonitor_collector CGroup (from CGroup stats)",
            button_label="All CPUs",
            y_axis_title="Time (%)",
            graph_type=GRAPH_TYPE_CGROUP,
            stack_state=False)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals


def choose_byte_divider(mem_total_bytes):
    divider = 1
    unit = 'Bytes'
    if mem_total_bytes > 9E9:
        divider = 1E9
        unit = 'GB'
    elif mem_total_bytes > 9E6:
        divider = 1E6
        unit = 'MB'
    #print("%d -> %s, %d" % (mem_total_bytes,unit, divider))
    return (divider, unit)


def generate_baremetal_memory(web, jdata, hostname):
    # if baremetal memory data was not collected, just return:
    if 'proc_meminfo' not in jdata[0]:
        return
    
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
    
    def meminfo_stat_to_bytes(value):
        # NOTE: all values collected are in kB
        #print("meminfo_stat_to_bytes: " + str(value))
        return value * 1000

    mem_total_bytes = meminfo_stat_to_bytes(jdata[0]['proc_meminfo']['MemTotal'])
    baremetal_memory_stats = GoogleChartsTable(['Timestamp', 'Used', 'Cached (DiskRead)', 'Free'])
    divider, unit = choose_byte_divider(mem_total_bytes)

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        if meminfo_stat_to_bytes(s['proc_meminfo']['MemTotal']) != mem_total_bytes:
            continue  # this is impossible AFAIK (hot swap of memory is not handled!!)
        
        #
        # NOTE: most tools like e.g. free -k just map:
        #
        #   free output |   corresponding /proc/meminfo fields
        # --------------+---------------------------------------
        #   Mem: total  |   MemTotal
        #   Mem: used   |   MemTotal - MemFree - Buffers - Cached - Slab
        #   Mem: free   |   MemFree             ^^^^^^^^^           ^^^^
        #                                        Buffers and Slab are close to zero 99% of the time
        #
        # see https://access.redhat.com/solutions/406773
        
        mf = meminfo_stat_to_bytes(s['proc_meminfo']['MemFree'])
        mc = meminfo_stat_to_bytes(s['proc_meminfo']['Cached'])
        
        baremetal_memory_stats.addRow([
                s['timestamp']['datetime'],
                int((mem_total_bytes - mf - mc) / divider),   # compute used memory
                int(mc / divider), # cached
                int(mf / divider), # free
            ])

    # Produce the javascript:
    g = GoogleChartsGraph(
            data=baremetal_memory_stats,  # Data
            graph_title='Memory usage in ' + unit + " (from baremetal stats)",
            button_label="Memory Usage",
            y_axis_title=unit,
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=True)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals


def generate_cgroup_memory(web, jheader, jdata, hostname):
    # if cgroup data was not collected, just return:
    if 'cgroup_memory_stats' not in jdata[0]:
        return
    
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
    if "memory_limit_bytes" in jheader["cgroup_config"]:
        mem_total_bytes = jheader["cgroup_config"]["memory_limit_bytes"]
    else:
        mem_total_bytes = jdata[0]['cgroup_memory_stats']['total_cache'] + \
                          jdata[0]['cgroup_memory_stats']['total_rss'] 
    cgroup_memory_stats = GoogleChartsTable(['Timestamp', 'Used', 'Cached (DiskRead)', 'Alloc Failures'])
    divider, unit = choose_byte_divider(mem_total_bytes)
    
    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        mu = s['cgroup_memory_stats']['total_rss']
        mc = s['cgroup_memory_stats']['total_cache']
        mfail = s['cgroup_memory_stats']['failcnt']
        cgroup_memory_stats.addRow([
                s['timestamp']['datetime'],
                mu / divider,
                mc / divider,
                mfail,
            ])

    # Produce the javascript:
    g = GoogleChartsGraph(
            data=cgroup_memory_stats,  # Data
            graph_title='Memory used by cmonitor_collector CGroup in ' + unit +  " (from CGroup stats)",
            button_label="Memory Usage",
            y_axis_title=[unit, "Alloc Failures"],
            graph_type=GRAPH_TYPE_CGROUP,
            stack_state=False,
            series_for_2nd_yaxis=[2]) # put "failcnt" on 2nd y axis
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals

def generate_load_avg(web, jheader, jdata, hostname):
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
     
    num_baremetal_cpus = int(jheader["lscpu"]["cpus"])
     
    load_avg_stats = GoogleChartsTable(['Timestamp', 'LoadAvg (1min)', 'LoadAvg (5min)', 'LoadAvg (15min)'])
    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        #
        # See https://linux.die.net/man/5/proc
        # and https://blog.appsignal.com/2018/03/28/understanding-system-load-and-load-averages.html
        #
        # "The load of a system is essentially the number of processes active at any given time. 
        #  When idle, the load is 0. When a process starts, the load is incremented by 1. 
        #  A terminating process decrements the load by 1. Besides running processes, 
        #  any process that's queued up is also counted. So, when one process is actively using the CPU, 
        #  and two are waiting their turn, the load is 3."
        #  ...
        # "Generally, single-core CPU can handle one process at a time. An average load of 1.0 would mean 
        #  that one core is busy 100% of the time. If the load average drops to 0.5, the CPU has been idle 
        #  for 50% of the time."

        # since kernel reports a percentage in range [0-n], where n= number of cores,
        # we remap that in range [0-100%]
        
        load_avg_stats.addRow([
                s['timestamp']['datetime'],
                100 * float(s['proc_loadavg']['load_avg_1min']) / num_baremetal_cpus,
                100 * float(s['proc_loadavg']['load_avg_5min']) / num_baremetal_cpus,
                100 * float(s['proc_loadavg']['load_avg_15min']) / num_baremetal_cpus
            ])

    # Produce the javascript:
    g = GoogleChartsGraph(
            data=load_avg_stats,  # Data
            graph_title='Average Load ' +  " (from baremetal stats)",
            button_label="Average Load",
            y_axis_title="Load (%)",
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False)
    web.write(g.toGoogleChartJS())
    g_graphs.append(g)    # register this graph into globals



# =======================================================================================================
# MAIN SCRIPT PREPARE DATA
# =======================================================================================================


def main_process_file(infile, outfile):
    
    # read the raw .json as text
    try:
        if infile[-8:] == '.json.gz':
            print("Loading gzipped JSON file %s" % infile)
            f = gzip.open(infile, 'rb')
            text = f.read()
            f.close()
            
            # in Python 3.5 the gzip returns a sequence of "bytes" and not a "str"
            if isinstance(text, bytes):
                text = text.decode('utf-8')
        else:
            print("Loading JSON file %s" % infile)
            f = open(infile, "r")
            text = f.read()
            f.close()
    except OSError as err:
        print("Error while opening input JSON file '%s': %s" % (infile, err))
        sys.exit(1)
    
    # fix up the end of the file if it is not complete
    ending = text[-3:-1]  # The last two character
    if ending == "},":  # the data capture is still running or halted
        text = text[:-2] + "\n ]\n}\n"
    
    # Convert the text to json and extract the stats
    entry = []
    entry = json.loads(text)  # convert text to JSON
    
    try:
        jheader = entry["header"]
        jdata = entry["samples"]  # removes outer parts so we have a list of snapshot dictionaries
    except:
        print("Unexpected JSON format. Aborting.")
        sys.exit(1)
    
    # - - - - - Start nchart functions
    next_graph_need_stacking = 0
    
    # These are flags used as function arguments
    stacked = 1
    unstacked = 0
    
    # initialise some useful content
    hostname = jheader['identity']['hostname'] 

    jdata_first_sample = jdata[0]
    logical_cpus_indexes = []
    for key in jdata_first_sample['stat']:
        if key.startswith('cpu') and key != 'cpu_total':
            cpuIdx = int(key[3:])
            # print("%s %s" %(key, cpuIdx))
            logical_cpus_indexes.append(cpuIdx) 
    print("Found %d CPUs in input file: %s" % (len(logical_cpus_indexes), ', '.join(str(x) for x in logical_cpus_indexes)))
    print("Found %d data samples" % len(jdata))
    
    # ----- MAIN SCRIPT CREAT WEB FILE -
    print("Opening output file %s" % outfile)
    web = open(outfile, "w")  # Open the output file
    nchart_start_js(web, 'Monitoring data for hostname ' + hostname)
    
    # JAVASCRIPT GRAPHS
    generate_baremetal_cpus(web, jdata, logical_cpus_indexes, hostname)
    generate_cgroup_cpus(web, jdata, logical_cpus_indexes, hostname)
    generate_baremetal_memory(web, jdata, hostname)
    generate_cgroup_memory(web, jheader, jdata, hostname)
    generate_network_traffic(web, jdata, hostname)
    generate_disks_io(web, jdata, hostname)
    generate_load_avg(web, jheader, jdata, hostname)
    print("All data samples parsed correctly")

    # if process_data_found:
    #    bubbleit(web, topprocs_title, topprocs,  'Top Processes Summary' + details, "TopSum")
    #    g = GoogleChartsGraph( top_header, top_data,  'Top Procs by CPU time' + details, "TopProcs",unstacked)
  
    # generate_filesystems(web, jdata)
    
    monitoring_summary = [
        ( "Version:", '<a href="https://github.com/f18m/cmonitor">cmonitor</a> ' + jheader["cmonitor"]["version"] ),
        ( "User:", jheader["cmonitor"]["username"] ),
        ( "Collected:", jheader["cmonitor"]["collecting"] ),
        ( "Started sampling at:", jdata_first_sample["timestamp"]["datetime"] + " (Local)" ),
        ( "Started sampling at:", jdata_first_sample["timestamp"]["UTC"] + " (UTC)" ),
        ( "Samples:", str(len(jdata)) ),
        ( "Sampling Interval (s):", str(jheader["cmonitor"]["sample_interval_seconds"]) ),
        ( "Total time sampled (s):", str(jheader["cmonitor"]["sample_interval_seconds"] * len(jdata)) )
    ]
    
    # NOTE: unfortunately some useful information like:
    #        - RAM memory model/speed
    #        - Disk model/speed
    #        - NIC model/speed
    #       will not be available from inside a container, which is where cmonitor_collector usually runs...
    #       so we mostly show CPU stats:
    all_disks = []
    if "disks" in jdata_first_sample:
        all_disks = jdata_first_sample["disks"].keys()
    all_netdevices = []
    if "network_interfaces" in jdata_first_sample :
        all_netdevices = jdata_first_sample["network_interfaces"].keys()
    monitored_summary = [
        ( "Hostname:", jheader["identity"]["hostname"] ),
        ( "CPU:", jheader["lscpu"]["model_name"] ),
        ( "BogoMIPS:", jheader["lscpu"]["bogomips"] ),
        ( "OS:", jheader["os_release"]["pretty_name"] ),
        ( "Monitored CPUs:", str(len(logical_cpus_indexes)) ),
        ( "Monitored Network Devices:", str(len(all_netdevices)) ),
        ( "Monitored Disks:", str(len(all_disks)) ),
    ]
    
    nchart_end_js(web, generate_config_js(jheader))
    
    # HTML
    nchart_start_html_body(web, hostname)
    nchart_append_html_table(web, "Monitoring Summary", monitoring_summary)
    nchart_append_html_table(web, "Monitored System Summary", monitored_summary)
    web.write('<p>NOTE: to zoom use left-click and drag; to reset view use right-click.</p>\n')
    nchart_end_html_body(web)

    print("Completed processing")
    web.close()

# =======================================================================================================
# MAIN
# =======================================================================================================


if __name__ == '__main__':
    cmd = sys.argv[0]
    infile = sys.argv[1]
    try:
        outfile = sys.argv[2]
    except:
        if infile[-8:] == '.json.gz':
            outfile = infile[:-8] + '.html'
        elif infile[-5:] == '.json':
            outfile = infile[:-5] + '.html'
        else:
            outfile = infile + '.html'

    main_process_file(infile, outfile)
