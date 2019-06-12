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

GRAPH_SOURCE_DATA_BAREMETAL = 1
GRAPH_SOURCE_DATA_CGROUP = 2

GRAPH_TYPE_AREA_CHART = 1
GRAPH_TYPE_BUBBLE_CHART = 2

SAVE_DEFLATED_JS_DATATABLES = True
JS_INDENT_SIZE = 2


# =======================================================================================================
# GLOBALs
# =======================================================================================================

g_num_generated_charts = 1
g_next_graph_need_stacking = 0

# =======================================================================================================
# GoogleChartsTimeSeries
# This is the table of 
#    t;Y1;Y2;...;YN 
# data points for a GoogleCharts graph that is representing the evolution of N quantities over time
# =======================================================================================================
    
class GoogleChartsTimeSeries(object):

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
# GoogleChartsGenericTable
# This is the NxM table of 
#    Y1_1;Y2_1;...;YN_1
#    ...
#    Y1_M;Y2_M;...;YN_M
# data points for a GoogleCharts graph for M different objects characterized by N features
# =======================================================================================================
    
class GoogleChartsGenericTable(object):

    def __init__(self, column_names):
        self.column_names = column_names  # must be a LIST of strings
        self.rows = []  # list of lists with values

    def addRow(self, row_data_list):
        assert(len(row_data_list) == len(self.column_names))
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
            file.write(','.join(r))

    def toJSONForJS(self):
        ret = '[[' # start 2D JSON array
       
        # convert all other columns:
        for colName in self.column_names:
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
                 combobox_label="",
                 combobox_entry="",
                 graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
                 graph_type=GRAPH_TYPE_AREA_CHART,
                 graph_title="", 
                 stack_state=False, 
                 y_axis_title="", 
                 series_for_2nd_yaxis=[],
                 data=[]):
        self.button_label = button_label
        self.combobox_label = combobox_label
        assert((len(self.button_label)==0 and len(self.combobox_label)>0) or
               (len(self.button_label)>0 and len(self.combobox_label)==0))
        self.combobox_entry = combobox_entry
        self.source_data = graph_source  # one of GRAPH_TYPE_BAREMETAL or GRAPH_TYPE_CGROUP
        self.graph_type = graph_type
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

        
    def genGoogleChartJS_AreaChart(self):
        ''' After the JavaScript line graph data is output, the data is terminated and the graph options set'''
        global g_next_graph_need_stacking
        
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
        
        ret_string = ''
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
        g_next_graph_need_stacking = self.stack_state
        if g_next_graph_need_stacking:
            ret_string += '  isStacked:  1\n'
            g_next_graph_need_stacking = 0
        else:
            ret_string += '  isStacked:  0\n'
            
        ret_string += '};\n'  # end of "options_%s" variable
        ret_string += '\n'
        ret_string += 'if (g_chart && g_chart.clearChart)\n'
        ret_string += '  g_chart.clearChart();\n'
        ret_string += 'g_chart = new google.visualization.AreaChart(document.getElementById("chart_master_div"));\n'
        ret_string += 'g_chart.draw(data_%s, options_%s);\n' % (self.js_name, self.js_name)
        ret_string += 'g_current_data = data_%s;\n' % (self.js_name)
        ret_string += 'g_current_options = options_%s;\n' % (self.js_name)
        return ret_string
    
    def genGoogleChartJS_BubbleChart(self):
        assert(len(self.y_axis_title) == 2)
        ret_string = ''
        ret_string += 'var options_%s = {\n' % (self.js_name)
        ret_string += '  explorer: { actions: ["dragToZoom", "rightClickToReset"], keepInBounds: true, maxZoomIn: 20.0 },\n'
        ret_string += '  chartArea: { left: "5%", width: "85%", top: "10%", height: "80%" },\n'
        ret_string += '  title: "%s",\n' % (self.graph_title)
        ret_string += '  hAxis: { title:"%s" },\n' % str(self.y_axis_title[0])
        ret_string += '  vAxis: { title:"%s" },\n' % str(self.y_axis_title[1])
        ret_string += '  sizeAxis: { maxSize: 200 },\n'
        ret_string += '  bubble: { textStyle: {fontSize: 15} }\n'
        ret_string += '};\n'  # end of "options_%s" variable
        ret_string += '\n'
        ret_string += 'if (g_chart && g_chart.clearChart)\n'
        ret_string += '  g_chart.clearChart();\n'
        ret_string += 'g_chart = new google.visualization.BubbleChart(document.getElementById("chart_master_div"));\n'
        ret_string += 'g_chart.draw(data_%s, options_%s);\n' % (self.js_name, self.js_name)
        ret_string += 'g_current_data = data_%s;\n' % (self.js_name)
        ret_string += 'g_current_options = options_%s;\n' % (self.js_name)
        return ret_string
        
    def toGoogleChartJS(self):
        global g_next_graph_need_stacking
        
        # generate the JS
        js_code_inner = self.data_table.toGoogleChartTable(self.js_name)
        
        if self.graph_type==GRAPH_TYPE_AREA_CHART:
            js_code_inner += self.genGoogleChartJS_AreaChart()
        else:
            js_code_inner += self.genGoogleChartJS_BubbleChart()

        js_code = 'function draw_%s() {\n' % (self.js_name)
        js_code += textwrap.indent(js_code_inner, ' ' * JS_INDENT_SIZE)

        if len(self.button_label)>0:
            # this will be activated by a button that should reset all comboboxes of the graph
            js_code += '  reset_combo_boxes();\n'
        
        js_code += '}\n' # end of draw_%s function
        js_code += '\n'

        return js_code
        


# =======================================================================================================
# HtmlOutputPage 
# This is able to produce a self-contained HTML page with embedded JavaScript to draw performance charts 
# =======================================================================================================

class HtmlOutputPage:

    def __init__(self, outfile, title):
        self.title = title
        self.outfile = outfile
        self.file = open(outfile, "w")  # Open the output file
        self.graphs = []

    def appendGoogleChart(self, chart):
        assert isinstance(chart, GoogleChartsGraph)
        self.graphs.append(chart)
        
    def startHtmlHead(self):
        ''' Write the head of the HTML webpage and start the JS section '''
        self.file.write('''<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
  <title>{pageTitle}</title>'''.format(pageTitle=self.title))
        
        self.file.write('''
  <style>
     html,body { height:85%; }
     body { background-color: #eaeaea; }
     h3 { margin: 0px; }
     ul { margin: 0 0 0 0;padding-left: 20px; }
     button { margin-bottom: 3px; }
     #hostname_span { background-color: white; color: red; padding: 4px; }
     #button_table { width:100%; border-collapse: collapse; }
     #button_table_col { border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px; }
     #chart_master_div { width:98%; height:85%; border: darkgrey; border-style: solid; border-width: 2px; margin-left: auto; margin-right: auto}
     #chart_master_inner_div { position: absolute; top: 50%; left: 50%; -ms-transform: translate(-50%, -50%); transform: translate(-50%, -50%); }
     #chart_master_inner_p { font-size: x-large; }
     #bottom_div { float:left; border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px; }
     #bottom_div h3 { font-size: medium; }
     #bottom_div li { font-size: smaller; }
     #bottom_table_val { font-family: monospace; }
  </style>
  <script type="text/javascript" src="https://cdn.jsdelivr.net/npm/pako@1.0.10/dist/pako.min.js"></script>
  <script type="text/javascript" src="https://www.google.com/jsapi"></script>
  <script type="text/javascript">
/* Load GoogleCharts: */
google.load("visualization", "1.1", {packages:["corechart"]});
google.setOnLoadCallback(setup_button_click_handlers);

/* The global chart object: */
var g_chart = null;

/* Currently selected data & options: */
var g_current_data = null;
var g_current_options = null;

/* The global window showing the configuration of all collected data: */
var g_configWindow = null;

/* Create a trigger for window resize that will redraw current chart */
var g_window_resize_timer = null;
window.addEventListener('resize', function(e){
  if (g_window_resize_timer) 
    clearTimeout(g_window_resize_timer);

  g_window_resize_timer = setTimeout(function() {
    if (g_current_data && g_current_options)
      g_chart.draw(g_current_data, g_current_options);
  }, 500);
});

/* Utility function used with combobox controls: */
function call_function_named(func_name) {
  eval(func_name + "()");
}

/* Utility function used to clear main graph: */
function clear_chart() {
  if (g_chart && g_chart.clearChart)
    g_chart.clearChart();
}

/* Utility function used to reset combobox controls: */
function reset_combo_boxes() {
  document.getElementById("select_combobox_baremetal_cpus").value = "clear_chart";
  document.getElementById("select_combobox_cgroup_cpus").value = "clear_chart";
}

''')
        # at this point we will generate all helper JS functions
    
    
    def endHtmlHead(self, config):
        ''' Finish the JS portion and HTML head tag '''
        
        # convert into JS all the charts that belong to this HTML document:
        for num, graph in enumerate(self.graphs, start=1):
            self.file.write(graph.toGoogleChartJS())
        
        # add all event listeners for button clicks:
        self.file.write('function setup_button_click_handlers() {\n')
        for num, graph in enumerate(self.graphs, start=1):
            if len(graph.combobox_label)==0:
                self.file.write('  document.getElementById("btn_draw_%s").addEventListener("click", draw_%s);\n' % (graph.js_name, graph.js_name))
            else:
                # this will be selected from a combobox, no need to hook into a button
                pass
        self.file.write('  document.getElementById("btn_show_config").addEventListener("click", show_config_window);\n')
        self.file.write('}\n')
        
        self.file.write(config)
        self.file.write('  </script>\n')
        self.file.write('</head>\n')
        
        
    
    def startHtmlBody(self, cgroupName, hostname):
        self.file.write('<body>\n')
        self.file.write('  <h1>Monitoring data collected from host <span id="hostname_span">' + hostname + '</span></h1>\n')
        self.file.write('  <div id="button_div">\n')
        self.file.write('  <table id="button_table">\n')
        
        # Table header row
        self.file.write('  <tr>\n')
        self.file.write('    <td id="button_table_col"></td><td id="button_table_col"><b>CGroup</b> (%s)</td>\n' % cgroupName)
        self.file.write('    <td id="button_table_col"><b>Baremetal</b> (Data collected from /proc)</td>\n')
        self.file.write('  </tr>\n')
        
        # Datarow
        self.file.write('  <tr>\n')
        self.file.write('  <td id="button_table_col">\n')
        self.file.write('    <button id="btn_show_config"><b>Configuration</b></button><br/>\n')
        self.file.write('  </td><td id="button_table_col">\n')
    
        def write_buttons_for_graph_type(source_data):
            # find all graphs that will be activated through a combobox
            graphs_combobox = {}
            for num, graph in enumerate(self.graphs, start=1):
                if graph.source_data == source_data and len(graph.combobox_label)>0:
                    if graph.combobox_label not in graphs_combobox:
                        # add new dict entry as empty list
                        graphs_combobox[graph.combobox_label] = []
                        
                    # add to the existing dict entry a new graph:
                    graphs_combobox[graph.combobox_label].append([ graph.combobox_entry, graph.js_name ])
                        
            # generate the CPU select box:
            if len(graphs_combobox)>0:
                for combobox_label in graphs_combobox.keys():
                    graph_list = graphs_combobox[combobox_label]
                    self.file.write('    <select id="select_combobox_%s" onchange="call_function_named(this.value)">\n' % (combobox_label))
                    self.file.write('      <option value="clear_chart">None</option>\n')
                    for entry in graph_list:
                        button_label = entry[0]
                        js_name = entry[1]
                        self.file.write('      <option value="draw_%s">%s</option>\n' % (js_name, button_label))
                    self.file.write('    </select>\n')
    
            # find in all graphs registered so far all those related to the CGROUP
            for num, graph in enumerate(self.graphs, start=1):
                if graph.source_data == source_data:
                    if len(graph.combobox_label)>0:
                        continue # skip - already drawn via <select>
                    elif 'CPU' in graph.button_label:
                        colour = 'red'
                    elif graph.button_label.startswith('Memory'):
                        colour = 'darkorange'
                    elif graph.button_label.startswith('Network'):
                        colour = 'darkblue'
                    elif graph.button_label.startswith('Disk'):
                        colour = 'darkgreen'
                    else:
                        colour = 'black'
                    self.file.write('    <button id="btn_draw_' + graph.js_name + '" style="color:' + colour + '"><b>' + graph.button_label + '</b></button>\n')
    
        write_buttons_for_graph_type(GRAPH_SOURCE_DATA_CGROUP)
        self.file.write('      </td><td id="button_table_col">\n')
        write_buttons_for_graph_type(GRAPH_SOURCE_DATA_BAREMETAL)
            
        self.file.write('  </td></tr>\n')
        self.file.write('  </table>\n')
        self.file.write('  </div>\n')
        self.file.write('  <p></p>\n')
        
        # finally generate the element where the main chart is going to be drawn:
        self.file.write('  <div id="chart_master_div"><div id="chart_master_inner_div"><p id="chart_master_inner_p">...click on a button above to show a graph...</p></div></div>\n')
    
        
    def appendHtmlTable(self, name, table_entries):
        self.file.write('  <div id="bottom_div">\n')
        self.file.write('    <h3>' + name + '</h3>\n')
        self.file.write('    <table>\n')
        self.file.write('    <tr><td><ul>\n')
        for i, entry in enumerate(table_entries, start=1):
            self.file.write("      <li>" + entry[0] + " <span id='bottom_table_val'>" + entry[1]+ "</span></li>\n")
            if (i % 4) == 0:
                self.file.write("      </ul></td><td><ul>\n")
        self.file.write('    </ul></td></tr>\n')
        self.file.write('    </table>\n')
        self.file.write('  </div>\n')
    
    
    def endHtmlBody(self):
        self.file.write('<p>NOTE: to zoom use left-click and drag; to reset view use right-click.</p>\n')
        self.file.write('</body>\n')
        self.file.write('</html>\n')
        self.file.close()



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
          
    if 'proc_meminfo' in jheader:
        jheader['proc_meminfo']['MemTotal'] = sizeof_fmt(int(jheader['proc_meminfo']['MemTotal']))
        jheader['proc_meminfo']['Hugepagesize'] = sizeof_fmt(int(jheader['proc_meminfo']['Hugepagesize']))

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
    if 'proc_meminfo' in jheader:
        config_str += configdump("proc_meminfo", "Memory Overview")
    #config_str += configdump("cpuinfo", "CPU Core Details")
    config_str += '      </table>\\\n'
    config_str += '      <h2>Monitoring Summary</h2>\\\n'
    config_str += '      <table>\\\n'
    config_str += configdump("cmonitor", "Performance Stats Collector Configuration")
    config_str += '      </table>\\\n'
    config_str += '    </body></html>\\\n'
    config_str += '");\n}\n\n'
    return config_str

def generate_topN_procs(web, header, jdata, numProcsToShow=20):
    # if process data was not collected, just return:
    if 'cgroup_tasks' not in jdata[0]:
        return web
    
    # build a dictionary containing cumulative metrics for CPU/IO/MEM data for each process
    # along all collected samples
    process_dict = {}
    max_mem_bytes = 0
    max_io_bytes = 0
    for sample in jdata:
        for process in sample['cgroup_tasks']:
            
            # parse data from JSON
            entry = sample['cgroup_tasks'][process]
            cmd = entry['cmd']
            cputime = entry['cpu_usr_total_secs'] + entry['cpu_sys_total_secs']
            iobytes = entry['io_total_read'] + entry['io_total_write']
            membytes = entry['mem_rss_bytes']    # take RSS, more realistic/useful compared to the "mem_virtual_bytes"
            thepid = entry['pid']
            
            # keep track of maxs:
            max_mem_bytes = max(membytes,max_mem_bytes)
            max_io_bytes = max(iobytes,max_io_bytes)
            
            if cputime > 0:
                try:  # update the current entry
                    process_dict[thepid]['cpu'] = cputime
                    process_dict[thepid]['io'] = iobytes
                    process_dict[thepid]['mem'] = membytes
                    process_dict[thepid]['cmd'] = cmd
                except:  # no current entry so add one
                    process_dict.update({thepid: { 'cpu': cputime,
                                                   'io': iobytes,
                                                   'mem': membytes,
                                                   'cmd': cmd} })
    
    # now sort all collected processes by the amount of CPU*memory used:
    # NOTE: sorted() will return just the sorted list of KEYs = PIDs
    def sort_key(d):
        #return process_dict[d]['cpu'] * process_dict[d]['mem']
        return process_dict[d]['cpu']
    topN_process_pids_list = sorted(process_dict, key=sort_key, reverse=True)
    
    # truncate to first N:
    topN_process_pids_list = topN_process_pids_list[0:numProcsToShow]
    
    mem_divider, mem_unit = choose_byte_divider(max_mem_bytes)
    io_divider, io_unit = choose_byte_divider(max_io_bytes)
    
    def get_nice_cmd(pid):
        return '%s (%d)' % (process_dict[pid]['cmd'], pid)

    # now select the N top processes and put their data in a GoogleChart table:
    topN_process_table = GoogleChartsGenericTable(['Command', 'CPU time', 'I/O ' + io_unit, 'Command', 'Memory ' + mem_unit])
    for i, pid in enumerate(topN_process_pids_list):
        p = process_dict[pid]
        nicecmd = get_nice_cmd(pid)
        
        print("Processing data for %d-th CPU-top-scorer process [%s]" % (i+1, nicecmd))
        topN_process_table.addRow([p['cmd'], p['cpu'], p['io']/io_divider, nicecmd, p['mem']/mem_divider])
    
    # generate the bubble chart graph:
    web.appendGoogleChart(GoogleChartsGraph(
                          button_label='CPU/Memory/Disk Bubbles',
                          graph_source=GRAPH_SOURCE_DATA_CGROUP,
                          graph_type=GRAPH_TYPE_BUBBLE_CHART,
                          graph_title="CPU/Disk total usage on X/Y axes; memory usage as bubble size (from cgroup stats)",
                          y_axis_title=["CPU time","I/O " + io_unit],
                          data=topN_process_table))

    # now that first pass is done, adjust units for IO & memory
    mem_divider, mem_unit = choose_byte_divider(header['proc_meminfo']['MemTotal'])
    
    # now generate instead a table of CPU/IO/MEMORY usage over time per process:
    process_table = {}
    for key in [ 'cpu', 'io', 'mem' ]:
        process_table[key] = GoogleChartsTimeSeries(['Timestamp'] + [get_nice_cmd(pid) for pid in topN_process_pids_list])
    for sample in jdata:
        row = {}
        for key in [ 'cpu', 'io', 'mem' ]:
            row[key] = [ sample['timestamp']['datetime'] ]
            
        for top_process_pid in topN_process_pids_list:
            #print(top_process_pid)
            json_key = 'pid_%s' % top_process_pid
            if json_key in sample['cgroup_tasks']:
                top_proc_sample = sample['cgroup_tasks'][json_key]
                row['cpu'].append(top_proc_sample['cpu_tot'])
                row['io'].append(top_proc_sample['io_rchar'] + top_proc_sample['io_wchar'])
                row['mem'].append(top_proc_sample['mem_rss_bytes'] / mem_divider)
                
                # keep track of maxs:
                max_mem_bytes = max(membytes,max_mem_bytes)
                max_io_bytes = max(iobytes,max_io_bytes)
            else:
                # probably this process was born later or dead earlier than this timestamp
                row['cpu'].append(0)
                row['io'].append(0)
                row['mem'].append(0)
            
        for key in [ 'cpu', 'io', 'mem' ]:
            process_table[key].addRow(row[key])

    # produce the 3 graphs "by process":
    web.appendGoogleChart(GoogleChartsGraph(
            data=process_table['cpu'],
            graph_title="CPU usage by process (from cgroup stats)",
            button_label="CPU by Process",
            y_axis_title="CPU (%)",
            graph_source=GRAPH_SOURCE_DATA_CGROUP,
            stack_state=False))
    web.appendGoogleChart(GoogleChartsGraph(
            data=process_table['io'],
            graph_title="IO usage by process (from cgroup stats)",
            button_label="IO by Process",
            y_axis_title="IO Read+Write (Bytes Per Sec)",
            graph_source=GRAPH_SOURCE_DATA_CGROUP,
            stack_state=False))
    web.appendGoogleChart(GoogleChartsGraph(
            data=process_table['mem'],
            graph_title="Memory usage by process (from cgroup stats)",
            button_label="Memory by Process",
            y_axis_title="RSS Memory (%s)" % mem_unit,
            graph_source=GRAPH_SOURCE_DATA_CGROUP,
            stack_state=False))
    return web


def generate_disks_io(web, jdata):
    # if disk data was not collected, just return:
    if 'disks' not in jdata[0]:
        return web
    
    all_disks = jdata[0]["disks"].keys()
    if len(all_disks) == 0:
        return web
    
    # see https://www.kernel.org/doc/Documentation/iostats.txt
    
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
    # Process JSON sample and fill the GoogleChartsTimeSeries() object
    #
    
    disk_table = GoogleChartsTimeSeries(diskcols)
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

    web.appendGoogleChart(GoogleChartsGraph(button_label='Disk I/O', 
                          graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
                          graph_title="Disk I/O (from baremetal stats)",
                          y_axis_title="MB",
                          data=disk_table))
    return web

# def generate_filesystems(web, jdata):
#     global self.graphs
#     fsstr = ""
#     for fs in jdata[0]["filesystems"].keys():
#         fsstr = fsstr + "'" + fs + "',"
#     fsstr = fsstr[:-1]
#     startHtmlHead_line_graph(web, fsstr)
#     for i, s in enumerate(jdata):
#         web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
#         for fs in s["filesystems"].keys():
#             web.write(", %.1f" % (s["filesystems"][fs]["fs_full_percent"]))
#         web.write("]\n")
#     web.appendGoogleChart(GoogleChartsGraph( 'File Systems Used percent')
#     return web


def generate_network_traffic(web, jdata):
    # if network traffic data was not collected, just return:
    if 'network_interfaces' not in jdata[0]:
        return web
    
    all_netdevices = jdata[0]["network_interfaces"].keys()
    if len(all_netdevices) == 0:
        return web
    
    netcols = ['Timestamp']
    for device in all_netdevices:
        netcols.append(str(device) + "+in")
        netcols.append(str(device) + "-out")

    # convert from bytes to MB
    divider = 1000 * 1000

    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTimeSeries() object
    #
    
    # MB/sec
    
    net_table = GoogleChartsTimeSeries(netcols)
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

    web.appendGoogleChart(GoogleChartsGraph(
            graph_title='Network Traffic in MB/s (from baremetal stats)',
            button_label='Network Traffic (MB/s)',
            y_axis_title="MB/s",
            graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
            stack_state=False,
            data=net_table))
    
            
    # PPS
    
    net_table = GoogleChartsTimeSeries(netcols)
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

    web.appendGoogleChart(GoogleChartsGraph(
            graph_title='Network Traffic in PPS (from baremetal stats)',
            button_label='Network Traffic (PPS)',
            y_axis_title="PPS",
            graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
            stack_state=False,
            data=net_table))
    return web

def generate_baremetal_cpus(web, jdata, logical_cpus_indexes):
    # if baremetal CPU data was not collected, just return:
    if 'stat' not in jdata[0]:
        return web

    # prepare empty tables
    baremetal_cpu_stats = {}
    for c in logical_cpus_indexes:
        baremetal_cpu_stats[c] = GoogleChartsTimeSeries(['Timestamp', 'User', 'Nice', 'System', 'Idle', 'I/O wait', 'Hard IRQ', 'Soft IRQ', 'Steal'])
        
    all_cpus_table = GoogleChartsTimeSeries(['Timestamp'] + [('CPU' + str(x)) for x in logical_cpus_indexes])
    
    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTimeSeries() object
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
        web.appendGoogleChart(GoogleChartsGraph(
                data=baremetal_cpu_stats[c],  # Data
                graph_title='Logical CPU ' + str(c) + " (from baremetal stats)",
                combobox_label="baremetal_cpus",
                combobox_entry="CPU" + str(c),
                y_axis_title="Time (%)",
                graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
                stack_state=True))

    # Also produce the "all CPUs" graph
    web.appendGoogleChart(GoogleChartsGraph(
            data=all_cpus_table,  # Data
            graph_title="All logical CPUs allowed in cmonitor_collector CGroup (from baremetal stats)",
            button_label="All CPUs",
            y_axis_title="Time (%)",
            graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
            stack_state=False))
    return web

def generate_cgroup_cpus(web, jdata, logical_cpus_indexes):
    if 'cgroup_cpuacct_stats' not in jdata[0]:
        return web  # cgroup mode not enabled at collection time!
        
    # prepare empty tables
    cpu_stats_table = {}
    for c in logical_cpus_indexes:
        cpu_stats_table[c] = GoogleChartsTimeSeries(['Timestamp', 'User', 'System'])
        
    all_cpus_table = GoogleChartsTimeSeries(['Timestamp'] + [('CPU' + str(x)) for x in logical_cpus_indexes])
        
    #
    # MAIN LOOP
    # Process JSON sample and fill the GoogleChartsTimeSeries() object
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
        web.appendGoogleChart(GoogleChartsGraph(
                data=cpu_stats_table[c],  # Data
                graph_title='Logical CPU ' + str(c) + " (from CGroup stats)",
                combobox_label="cgroup_cpus",
                combobox_entry="CPU" + str(c),
                y_axis_title="Time (%)",
                graph_source=GRAPH_SOURCE_DATA_CGROUP,
                stack_state=True))

    # Also produce the "all CPUs" graph
    web.appendGoogleChart(GoogleChartsGraph(
            data=all_cpus_table,  # Data
            graph_title="All logical CPUs assigned to cmonitor_collector CGroup (from CGroup stats)",
            button_label="All CPUs",
            y_axis_title="Time (%)",
            graph_source=GRAPH_SOURCE_DATA_CGROUP,
            stack_state=False))
    return web

def generate_baremetal_memory(web, jdata):
    # if baremetal memory data was not collected, just return:
    if 'proc_meminfo' not in jdata[0]:
        return web
    
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
    
    mem_total_bytes = jdata[0]['proc_meminfo']['MemTotal']
    baremetal_memory_stats = GoogleChartsTimeSeries(['Timestamp', 'Used', 'Cached (DiskRead)', 'Free'])
    divider, unit = choose_byte_divider(mem_total_bytes)

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        meminfo_stats = s['proc_meminfo']
        
        if meminfo_stats['MemTotal'] != mem_total_bytes:
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
        
        mf = meminfo_stats['MemFree']
        mc = meminfo_stats['Cached']
        
        baremetal_memory_stats.addRow([
                s['timestamp']['datetime'],
                int((mem_total_bytes - mf - mc) / divider),   # compute used memory
                int(mc / divider), # cached
                int(mf / divider), # free
            ])

    # Produce the javascript:
    web.appendGoogleChart(GoogleChartsGraph(
            data=baremetal_memory_stats,  # Data
            graph_title='Memory usage in ' + unit + " (from baremetal stats)",
            button_label="Memory Usage",
            y_axis_title=unit,
            graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
            stack_state=True))
    return web


def generate_cgroup_memory(web, jheader, jdata):
    # if cgroup data was not collected, just return:
    if 'cgroup_memory_stats' not in jdata[0]:
        return web
    
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
    cgroup_memory_stats = GoogleChartsTimeSeries(['Timestamp', 'Used', 'Cached (DiskRead)', 'Alloc Failures'])
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
    web.appendGoogleChart(GoogleChartsGraph(
            data=cgroup_memory_stats,  # Data
            graph_title='Memory used by cmonitor_collector CGroup in ' + unit +  " (from CGroup stats)",
            button_label="Memory Usage",
            y_axis_title=[unit, "Alloc Failures"],
            graph_source=GRAPH_SOURCE_DATA_CGROUP,
            stack_state=False,
            series_for_2nd_yaxis=[2])) # put "failcnt" on 2nd y axis
    return web


def generate_load_avg(web, jheader, jdata):
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
     
    num_baremetal_cpus = int(jheader["lscpu"]["cpus"])
     
    load_avg_stats = GoogleChartsTimeSeries(['Timestamp', 'LoadAvg (1min)', 'LoadAvg (5min)', 'LoadAvg (15min)'])
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
    web.appendGoogleChart(GoogleChartsGraph(
            data=load_avg_stats,  # Data
            graph_title='Average Load ' +  " (from baremetal stats)",
            button_label="Average Load",
            y_axis_title="Load (%)",
            graph_source=GRAPH_SOURCE_DATA_BAREMETAL,
            stack_state=False))
    return web

def generate_monitoring_summary(jheader, jdata):
    jdata_first_sample = jdata[0]
    monitoring_summary = [
        ( "Version:", '<a href="https://github.com/f18m/cmonitor">cmonitor</a> ' + jheader["cmonitor"]["version"] ),
        #( "User:", jheader["cmonitor"]["username"] ),   # not really useful
        ( "Collected:", jheader["cmonitor"]["collecting"] ),
        #( "Started sampling at:", jdata_first_sample["timestamp"]["datetime"] + " (Local)" ),   # not really useful
        ( "Started sampling at:", jdata_first_sample["timestamp"]["UTC"] + " (UTC)" ),
        ( "Samples:", str(len(jdata)) ),
        ( "Sampling Interval (s):", str(jheader["cmonitor"]["sample_interval_seconds"]) ),
        ( "Total time sampled (hh:mm:ss):", str(datetime.timedelta(seconds = jheader["cmonitor"]["sample_interval_seconds"]*len(jdata))) )
    ]
    return monitoring_summary

def generate_monitored_summary(jheader, jdata, logical_cpus_indexes):
    jdata_first_sample = jdata[0]
    
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
        ( "OS:", jheader["os_release"]["pretty_name"] ),
        ( "CPU:", jheader["lscpu"]["model_name"] ),
        ( "BogoMIPS:", jheader["lscpu"]["bogomips"] ),
        ( "Monitored CPUs:", str(len(logical_cpus_indexes)) ),
        ( "Monitored Disks:", str(len(all_disks)) ),
        ( "Monitored Network Devices:", str(len(all_netdevices)) ),
    ]
    return monitored_summary



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
    entry = json.loads(text)  # convert text to JSON
    try:
        jheader = entry["header"]
        jdata = entry["samples"]  # removes outer parts so we have a list of snapshot dictionaries
    except:
        print("Unexpected JSON format. Aborting.")
        sys.exit(1)
    
    # initialise some useful content
    hostname = jheader['identity']['hostname'] 
    jdata_first_sample = jdata[0]
    print("Found %d data samples" % len(jdata))

    # detect num of CPUs:
    logical_cpus_indexes = []
    for key in jdata_first_sample['stat']:
        if key.startswith('cpu') and key != 'cpu_total':
            cpuIdx = int(key[3:])
            # print("%s %s" %(key, cpuIdx))
            logical_cpus_indexes.append(cpuIdx) 
    print("Found %d CPUs in input file: %s" % (len(logical_cpus_indexes), ', '.join(str(x) for x in logical_cpus_indexes)))
    
    print("Opening output file %s" % outfile)
    web = HtmlOutputPage(outfile, 'Monitoring data for hostname ' + hostname)

    # HTML HEAD
    
    web.startHtmlHead()
    web = generate_baremetal_cpus(web, jdata, logical_cpus_indexes)
    web = generate_cgroup_cpus(web, jdata, logical_cpus_indexes)
    web = generate_baremetal_memory(web, jdata)
    web = generate_cgroup_memory(web, jheader, jdata)
    web = generate_network_traffic(web, jdata)
    web = generate_disks_io(web, jdata)
    web = generate_load_avg(web, jheader, jdata)
    web = generate_topN_procs(web, jheader, jdata)
    web.endHtmlHead(generate_config_js(jheader))
    
    print("All data samples parsed correctly")

    
    # HTML BODY
    
    if 'cgroup_config' in jheader and 'name' in jheader['cgroup_config']:
        cgroupName = jheader['cgroup_config']['name']
    else:
        cgroupName = 'None'
    web.startHtmlBody(cgroupName, hostname)
    web.appendHtmlTable("Monitoring Summary", generate_monitoring_summary(jheader, jdata))
    web.appendHtmlTable("Monitored System Summary", generate_monitored_summary(jheader, jdata, logical_cpus_indexes))
    web.endHtmlBody()

    print("Completed processing")


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
