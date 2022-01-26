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
import argparse
import getopt
import os
import time
from cmonitor_loader import CmonitorCollectorJsonLoader
from cmonitor_version import VERSION_STRING

# =======================================================================================================
# CONSTANTS
# =======================================================================================================

GRAPH_SOURCE_DATA_IS_BAREMETAL = 1
GRAPH_SOURCE_DATA_IS_CGROUP_TOTALS = 2
GRAPH_SOURCE_DATA_IS_CGROUP_PER_PROCESS = 3

GRAPH_TYPE_AREA_CHART = 1
GRAPH_TYPE_BUBBLE_CHART = 2

SAVE_DEFLATED_JS_DATATABLES = True
JS_INDENT_SIZE = 2

# see https://developers.google.com/chart/interactive/docs/reference#dateformat
# the idea is that cmonitor_chart will most likely be used to explore short time intervals
# so that day/month/year part is not useful, just the time is useful; in tooltip we also
# reach millisec accuracy:
X_AXIS_DATEFORMAT = "HH:mm:ss"
TOOLTIP_DATEFORMAT = "HH:mm:ss.SSS z"


# =======================================================================================================
# GLOBALs
# =======================================================================================================

verbose = False
g_num_generated_charts = 1
g_next_graph_need_stacking = 0
g_datetime = "localtz"  # can be changed to "UTC" with --utc; FIXME currently we always use just UTC, never localtz...

# =======================================================================================================
# GoogleChartsTimeSeries
# =======================================================================================================


class GoogleChartsTimeSeries(object):
    """
    GoogleChartsTimeSeries is a (N+1)xM table of
       t_1;Y1_1;Y2_1;...;YN_1
       t_2;Y1_2;Y2_2;...;YN_2
       ...
       t_M;Y1_M;Y2_M;...;YN_M
    data points for a GoogleCharts graph that is representing the evolution of N quantities over time
    """

    def __init__(self, column_names, column_units=None):
        self.column_names = column_names  # must be a LIST of strings
        self.column_units = column_units
        if self.column_units:
            assert len(self.column_units) == len(self.column_names)
        self.rows = []  # list of lists with values

    def ISOdatetimeToJSDate(self, date):
        """convert ISO datetime strings like
          "2017-08-21T20:12:30.123"
        to strings like:
          "Date(2017,8,21,20,12,30,123000)"
        which are the datetime representation suitable for JS GoogleCharts, see
        https://developers.google.com/chart/interactive/docs/datesandtimes
        """
        dateAsPythonObj = datetime.datetime.strptime(date, "%Y-%m-%dT%H:%M:%S.%f")

        return "Date(%d,%d,%d,%d,%d,%d,%d)" % (
            dateAsPythonObj.year,
            dateAsPythonObj.month,
            dateAsPythonObj.day,
            dateAsPythonObj.hour,
            dateAsPythonObj.minute,
            dateAsPythonObj.second,
            dateAsPythonObj.microsecond / 1000,  # NOTE: the JavaScript Date() object wants milliseconds
        )

    def addRow(self, row_data_list):
        assert len(row_data_list) == len(self.column_names)

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
        assert len(self.column_names) >= 2
        return len(self.column_names) - 1

    def getMaxValueDataSerie(self, column_index):
        # WARNING: this looks very inefficient!
        assert column_index >= 0 and column_index <= len(self.column_names) - 1
        ret = 0
        for r in self.rows:
            ret = max(ret, r[1 + column_index])
        return ret

    def getDataSeriesIndexByName(self, column_name):
        assert column_name != self.column_names[0]  # the first column is not a "data serie", it's the timestamp column!
        try:
            col_idx = self.column_names.index(column_name)
            assert col_idx >= 1
            return col_idx - 1  # the first data serie, with index 0, is the column immediately after the timestamp column
        except ValueError:
            # column name not found
            return -1

    def writeTo(self, file):
        for r in self.rows:
            # assume first column is always the timestamp:
            row_text = "['Date(%s)'," % r[0]
            row_text += ",".join(str(x) for x in r[1:])
            row_text += "],\n"
            file.write(row_text)

    def toJSONForJS(self):
        ret = "[["  # start 2D JSON array

        # convert 1st column:
        assert self.column_names[0] == "Timestamp"
        ret += '{"type":"datetime","label":"Datetime"},'

        # convert all other columns:
        for colName in self.column_names[1:]:
            ret += '"' + colName + '",'
        ret = ret[:-1]

        # separe first line; start conversion of actual table data:
        ret += "],"

        data = json.dumps(self.rows, separators=(",", ":"))
        data = data[1:]

        return ret + data

    def toDeflatedJSONBase64Encoded(self):
        """Returns this table in JSON format (for JS), deflated using zlib, and represented as a Base64-encoded ASCII string"""
        json_string = self.toJSONForJS()
        json_compressed_bytearray = zlib.compress(json_string.encode(), 9)

        ret = str(binascii.b2a_base64(json_compressed_bytearray))
        return ret[1:]

    def toGoogleChartTable(self, graphName):
        """Writes in the given file the JavaScript GoogleCharts object representing this table"""
        ret_string = ""
        if SAVE_DEFLATED_JS_DATATABLES:
            # to reduce the HTML size save the deflated, serialized JSON of the 2D JS array:
            ret_string += "var deflated_data_base64_%s = %s;\n" % (
                graphName,
                self.toDeflatedJSONBase64Encoded(),
            )

            # then convert it base64 -> JS binary string
            ret_string += "var deflated_data_binary_%s = window.atob(deflated_data_base64_%s);\n" % (graphName, graphName)

            # now inflate it in the browser using "pako" library (https://github.com/nodeca/pako)
            ret_string += "var inflated_data_%s = JSON.parse(pako.inflate(deflated_data_binary_%s, { to: 'string' }));\n" % (graphName, graphName)
        else:
            ret_string += "var inflated_data_%s = %s;\n" % (
                graphName,
                self.toJSONForJS(),
            )

        # finally create the GoogleCharts table from it:
        ret_string += "var data_%s = google.visualization.arrayToDataTable(inflated_data_%s);\n\n" % (graphName, graphName)

        # add DateFormatter to use custom formatting of the 1st column (like everywhere else we assume first column is the timestamp)
        ret_string += "var date_formatter = new google.visualization.DateFormat({pattern: '%s'});\n" % (TOOLTIP_DATEFORMAT)
        ret_string += "date_formatter.format(data_%s, 0);\n" % (graphName)

        if self.column_units:
            column_units_strings = ["'" + v + "'" for v in self.column_units]

            # add Javascript code to set the formatted value on EACH and EVERY single entry of the table (except timestamp);
            # this improves greatly the readability of TOOLTIPs generated by Google Charts: instead of showing very large numbers
            # they will show up nice "k", "M" and "G" units
            ret_string += """
var column_unit = [%s]
for (var c=1; c < data_%s.getNumberOfColumns(); c++) {
    for (var r=0; r < data_%s.getNumberOfRows(); r++) {
        var v = data_%s.getValue(r, c);
        data_%s.setFormattedValue(r, c, prettyPrinter(v) + column_unit[c]);
    }
}

""" % (
                ",".join(column_units_strings),
                graphName,
                graphName,
                graphName,
                graphName,
            )

        return ret_string


# =======================================================================================================
# GoogleChartsGenericTable
# =======================================================================================================


class GoogleChartsGenericTable(object):
    """
    This is the NxM table of
       Y1_1;Y2_1;...;YN_1
       ...
       Y1_M;Y2_M;...;YN_M
    data points for a GoogleCharts graph for M different objects characterized by N features.
    This class is useful to create graphs which are NOT related to a measurement that evolves over TIME.

    Currently this class is used only for the generation of bubble charts, which are, by their nature,
    suited to represent relationships among different features (in our case total IO, memory and CPU usage)
    """

    def __init__(self, column_names):
        self.column_names = column_names  # must be a LIST of strings
        self.rows = []  # list of lists with values

    def addRow(self, row_data_list):
        assert len(row_data_list) == len(self.column_names)
        self.rows.append(row_data_list)

    def getRow(self, index):
        return self.rows[index]

    def getListColumnNames(self):
        return self.column_names

    def getNumDataSeries(self):
        # assuming first column is the timestamp, the number of "data series"
        # present in this table is all remaining columns
        return len(self.column_names) - 1

    def writeTo(self, file):
        for r in self.rows:
            file.write(",".join(r))

    def toJSONForJS(self):
        ret = "[["  # start 2D JSON array

        # convert all other columns:
        for colName in self.column_names:
            ret += '"' + colName + '",'
        ret = ret[:-1]

        # separe first line; start conversion of actual table data:
        ret += "],"

        data = json.dumps(self.rows, separators=(",", ":"))
        data = data[1:]

        return ret + data

    def toDeflatedJSONBase64Encoded(self):
        """Returns this table in JSON format (for JS), deflated using zlib, and represented as a Base64-encoded ASCII string"""
        json_string = self.toJSONForJS()
        json_compressed_bytearray = zlib.compress(json_string.encode(), 9)

        ret = str(binascii.b2a_base64(json_compressed_bytearray))
        return ret[1:]

    def toGoogleChartTable(self, graphName):
        """Writes in the given file the JavaScript GoogleCharts object representing this table"""
        ret_string = ""
        if SAVE_DEFLATED_JS_DATATABLES:
            # to reduce the HTML size save the deflated, serialized JSON of the 2D JS array:
            ret_string += "var deflated_data_base64_%s = %s;\n" % (
                graphName,
                self.toDeflatedJSONBase64Encoded(),
            )

            # then convert it base64 -> JS binary string
            ret_string += "var deflated_data_binary_%s = window.atob(deflated_data_base64_%s);\n" % (graphName, graphName)

            # now inflate it in the browser using "pako" library (https://github.com/nodeca/pako)
            ret_string += "var inflated_data_%s = JSON.parse(pako.inflate(deflated_data_binary_%s, { to: 'string' }));\n" % (graphName, graphName)
        else:
            ret_string += "var inflated_data_%s = %s;\n" % (
                graphName,
                self.toJSONForJS(),
            )

        # finally create the GoogleCharts table from it:
        ret_string += "var data_%s = google.visualization.arrayToDataTable(inflated_data_%s);\n" % (graphName, graphName)
        return ret_string


# =======================================================================================================
# GoogleChartsGraph
# =======================================================================================================


class GoogleChartsGraph:
    """
    This is a simple object that can generate a JavaScript snippet (to be embedded in HTML output page)
    that will render at runtime a GoogleChart drawing inside a JavaScript-enabled browser of course.

    It supports Google AreaChart (see https://developers.google.com/chart/interactive/docs/gallery/areachart)
    with 1 or 2 Y axes. The data series that are placed on the 2nd Y axis are higlighted automatically by
    using a tick RED line.
    """

    def __init__(
        self,
        data=None,
        button_label="",
        combobox_label="",
        combobox_entry="",
        graph_source=GRAPH_SOURCE_DATA_IS_BAREMETAL,
        graph_type=GRAPH_TYPE_AREA_CHART,
        graph_title="",
        stack_state=False,
        y_axes_titles=[],
        y_axes_max_value=[None],
        columns_for_2nd_yaxis=None,
    ):
        self.data_table = data  # of type GoogleChartsGenericTable or GoogleChartsTimeSeries
        self.button_label = button_label
        self.combobox_label = combobox_label
        assert (len(self.button_label) == 0 and len(self.combobox_label) > 0) or (len(self.button_label) > 0 and len(self.combobox_label) == 0)
        self.combobox_entry = combobox_entry
        self.source_data = graph_source  # one of GRAPH_TYPE_BAREMETAL or GRAPH_TYPE_CGROUP
        self.graph_type = graph_type
        self.graph_title = graph_title
        self.stack_state = stack_state
        self.y_axes_titles = y_axes_titles
        self.columns_for_2nd_yaxis = columns_for_2nd_yaxis
        self.y_axes_max_value = y_axes_max_value
        self.graph_title += ", STACKED graph" if self.stack_state else ""

        # generate new JS name for this graph
        global g_num_generated_charts
        self.js_name = "graph" + str(g_num_generated_charts)
        g_num_generated_charts += 1

    def __genGoogleChartJS_AreaChart(self):
        """After the JavaScript line graph data is output, the data is terminated and the graph options set"""
        global g_next_graph_need_stacking

        def __internalWriteSeries(series_indexes, target_axis_index):
            ret = ""
            for i, idx in enumerate(series_indexes, start=0):
                if target_axis_index == 0:
                    ret += "   %d: {targetAxisIndex:%d}" % (idx, target_axis_index)
                else:
                    # IMPORTANT: the data series that go on the 2nd Y axis (typically just one) are drawn with a RED thick line
                    #            to underline their importance; area opacity is removed to avoid clutter with data series on the first Y axis
                    ret += "   %d: {targetAxisIndex:%d, lineWidth: 5, areaOpacity: 0, color: 'red', lineDashStyle: [10,2]}" % (
                        idx,
                        target_axis_index,
                    )
                # print("i=%d, idx=%d, target_axis_index=%d" % (i,idx,target_axis_index))
                if i < len(series_indexes):
                    ret += ",\n"
                else:
                    ret += "\n"
            return ret

        def __internalWriteVAxis(v_axis_idx, max_value, title, data_series_indexes):
            ret = ""
            if max_value is None:
                # let Google Chart automatically determine min/max on this axis
                ret += '    %d: { title: "%s", format: "short" },\n' % (v_axis_idx, title)
            elif max_value == 0:
                # autocompute the best MAX
                actual_max = 0
                for idx in data_series_indexes:
                    actual_max = max(actual_max, self.data_table.getMaxValueDataSerie(idx))
                ret += '    %d: { title: "%s", format: "short", minValue: -1, maxValue: %d },\n' % (v_axis_idx, title, actual_max * 5 + 10)
            else:
                ret += '    %d: { title: "%s", format: "short", minValue: -1, maxValue: %d },\n' % (v_axis_idx, title, max_value)
            return ret

        ret_string = ""
        ret_string += "var options_%s = {\n" % (self.js_name)
        ret_string += '  chartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n'
        ret_string += '  title: "%s",\n' % (self.graph_title)
        ret_string += '  focusTarget: "category",\n'

        # by default this tool plots the top 20 processes; in these cases both tooltips and legend will have up to 21 rows (including time)
        # so we make the font a bit smaller to make it more likely to view all the lines
        ret_string += "  tooltip: { textStyle: { fontSize: 12 } },\n"
        ret_string += "  legend: { textStyle: { fontSize: 12 } },\n"
        ret_string += '  explorer: { actions: ["dragToZoom", "rightClickToReset"], keepInBounds: true, maxZoomIn: 20.0 },\n'

        # HORIZONTAL AXIS
        ret_string += '  hAxis: { format: "%s", gridlines: { color: "lightgrey", count: 30 } },\n' % X_AXIS_DATEFORMAT

        # VERTICAL AXIS (OR AXES)
        if self.columns_for_2nd_yaxis:
            # compute indexes of series that use the 2nd Y axis:
            series_for_2nd_yaxis = []
            for colname in self.columns_for_2nd_yaxis:
                idx = self.data_table.getDataSeriesIndexByName(colname)
                assert idx != -1, f"Column named {colname} is not a column inside the data table!"
                series_for_2nd_yaxis.append(idx)
            # print("series_for_2nd_yaxis: %s" % ",".join(str(x) for x in series_for_2nd_yaxis))

            # compute indexes of series that use 1st Y axis:
            all_indexes = range(0, self.data_table.getNumDataSeries())
            series_for_1st_yaxis = [idx for idx in all_indexes if idx not in series_for_2nd_yaxis]
            # print("series_for_1st_yaxis: %s" % ",".join(str(x) for x in series_for_1st_yaxis))

            # assign data series to the 2 Y axes:
            ret_string += "  series: {\n"
            ret_string += __internalWriteSeries(series_for_1st_yaxis, 0)
            ret_string += __internalWriteSeries(series_for_2nd_yaxis, 1)
            ret_string += "  },\n"

            # check data
            assert len(self.y_axes_titles) == 2
            assert len(self.y_axes_max_value) == 2, f"Got {self.y_axes_max_value}, but columns_for_2nd_yaxis={self.columns_for_2nd_yaxis}"

            # allocate 2 Y axes:
            ret_string += "  vAxes: {\n"
            ret_string += __internalWriteVAxis(0, self.y_axes_max_value[0], self.y_axes_titles[0], series_for_1st_yaxis)
            ret_string += __internalWriteVAxis(1, self.y_axes_max_value[1], self.y_axes_titles[1], series_for_2nd_yaxis)
            ret_string += "  },\n"
        else:
            # single vertical axis:
            assert len(self.y_axes_titles) == 1
            ret_string += '  vAxis: { title: "%s", format: "short", gridlines: { color: "lightgrey", count: 11 } },\n' % str(self.y_axes_titles[0])

        # graph stacking
        g_next_graph_need_stacking = self.stack_state
        if g_next_graph_need_stacking:
            ret_string += "  isStacked:  1\n"
            g_next_graph_need_stacking = 0
        else:
            ret_string += "  isStacked:  0\n"

        ret_string += "};\n"  # end of "options_%s" variable
        ret_string += "\n"
        ret_string += "set_main_chart_div_as_visible();\n"
        ret_string += "if (g_chart && g_chart.clearChart)\n"
        ret_string += "  g_chart.clearChart();\n"
        ret_string += 'g_chart = new google.visualization.AreaChart(document.getElementById("chart_master_div"));\n'

        # immediately before drawing the chart, add a listener to hack some ugly labeling by Google Charts
        ret_string += "google.visualization.events.addListener(g_chart, 'ready', fix_vaxis_ticks);\n"
        ret_string += "g_chart.draw(data_%s, options_%s);\n" % (
            self.js_name,
            self.js_name,
        )

        ret_string += "g_current_data = data_%s;\n" % (self.js_name)
        ret_string += "g_current_options = options_%s;\n" % (self.js_name)

        # this graph will be activated by either
        #  - a button that should reset all comboboxes of the page
        #  - a combo box entry that should reset all other comboboxes in the page
        ret_string += 'reset_combo_boxes("%s");\n' % self.combobox_label

        return ret_string

    def __genGoogleChartJS_BubbleChart(self):
        assert len(self.y_axes_titles) == 2
        ret_string = ""
        ret_string += "var options_%s = {\n" % (self.js_name)
        ret_string += '  explorer: { actions: ["dragToZoom", "rightClickToReset"], keepInBounds: true, maxZoomIn: 20.0 },\n'
        ret_string += '  chartArea: { left: "5%", width: "85%", top: "10%", height: "80%" },\n'
        ret_string += '  title: "%s",\n' % (self.graph_title)
        ret_string += '  hAxis: { title:"%s" },\n' % str(self.y_axes_titles[0])
        ret_string += '  vAxis: { title:"%s", format:"short" },\n' % str(self.y_axes_titles[1])
        ret_string += "  sizeAxis: { maxSize: 200 },\n"
        ret_string += "  bubble: { textStyle: {fontSize: 15} }\n"
        ret_string += "};\n"  # end of "options_%s" variable
        ret_string += "\n"
        ret_string += "if (g_chart && g_chart.clearChart)\n"
        ret_string += "  g_chart.clearChart();\n"
        ret_string += "set_main_chart_div_as_visible();\n"
        ret_string += 'g_chart = new google.visualization.BubbleChart(document.getElementById("chart_master_div"));\n'
        ret_string += "g_chart.draw(data_%s, options_%s);\n" % (
            self.js_name,
            self.js_name,
        )
        ret_string += "g_current_data = data_%s;\n" % (self.js_name)
        ret_string += "g_current_options = options_%s;\n" % (self.js_name)
        return ret_string

    def toGoogleChartJS(self):
        global g_next_graph_need_stacking

        # generate the JS
        js_code_inner = self.data_table.toGoogleChartTable(self.js_name)

        if self.graph_type == GRAPH_TYPE_AREA_CHART:
            js_code_inner += self.__genGoogleChartJS_AreaChart()
        else:
            js_code_inner += self.__genGoogleChartJS_BubbleChart()

        js_code = "function draw_%s() {\n" % (self.js_name)
        js_code += textwrap.indent(js_code_inner, " " * JS_INDENT_SIZE)
        js_code += "}\n"  # end of draw_%s function
        js_code += "\n"

        return js_code


# =======================================================================================================
# HtmlOutputPage
# =======================================================================================================


class HtmlOutputPage:
    """
    This is able to produce a self-contained HTML page with embedded JavaScript to draw performance charts
    """

    def __init__(self, outfile, title):
        self.title = title
        self.outfile = outfile
        self.file = open(outfile, "w")  # Open the output file
        self.graphs = []

    def appendGoogleChart(self, chart):
        assert isinstance(chart, GoogleChartsGraph)
        self.graphs.append(chart)

    def writeHtmlHead(self):
        """Write the head of the HTML webpage and start the JS section"""
        self.file.write(
            """<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<head>
  <title>{pageTitle}</title>""".format(
                pageTitle=self.title
            )
        )

        self.file.write(
            """
  <style>
     html,body { height:85%; }
     body { background-color: #eaeaea; }
     h3 { margin: 0px; }
     ul { margin: 0 0 0 0;padding-left: 20px; }
     button { margin-bottom: 3px; }
     #monitored_system_span { background-color: white; color: red; padding: 4px; }
     #button_table { width:100%; border-collapse: collapse; margin-bottom: 5px; }
     .button_table_col { border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px; }
     #config_viewer_div { width:98%; height:85%; border: darkgrey; border-style: solid; border-width: 2px; margin-left: auto; margin-right: auto; overflow: auto}
     .config_sectioncol {font-weight: bold; padding: 1ex; font-size: large;background-color: lightsteelblue;}
     .config_key {font-weight: bold;}
     .config_val {font-family: monospace;}
     #chart_master_div { width:98%; height:85%; border: darkgrey; border-style: solid; border-width: 2px; margin-left: auto; margin-right: auto}
     #chart_master_inner_div { position: absolute; top: 50%; left: 50%; -ms-transform: translate(-50%, -50%); transform: translate(-50%, -50%); }
     #chart_master_inner_p { font-size: x-large; }
     .bottom_div { float:left; max-width: 40%; border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px; }
     .bottom_div h3 { font-size: medium; }
     .bottom_div li { font-size: smaller; }
     .bottom_about_div { float:right; max-width: 15%; border: darkgrey; border-style: solid; border-width: 2px; padding: 6px; margin: 6px; }
     .bottom_about_div h3 { font-size: medium; }
     .bottom_about_div li { font-size: smaller; }
     .bottom_table_val { font-family: monospace; }
  </style>
  <script type="text/javascript" src="https://cdn.jsdelivr.net/npm/pako@1.0.10/dist/pako.min.js"></script>
  <script type="text/javascript" src="https://www.gstatic.com/charts/loader.js"></script>
  <script type="text/javascript">
// Load the Visualization API and the corechart package; version 51 is June 2021 version
google.charts.load('51', {'packages':['corechart']});
google.setOnLoadCallback(on_load_callback);

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

/* Utility function used to put SI measurement unit on byte values appearing inside tooltips */
function prettyPrinter(num) {
     if (num >= 1000000000) {
        return (num / 1000000000).toFixed(1).replace(/\.0$/, '') + 'G';
     }
     if (num >= 1000000) {
        return (num / 1000000).toFixed(1).replace(/\.0$/, '') + 'M';
     }
     if (num >= 1000) {
        return (num / 1000).toFixed(1).replace(/\.0$/, '') + 'K';
     }
     return num;
}

/* Utility function to use the 'ready' event to modify the chart once it has been drawn */
function fix_vaxis_ticks() {
    var chart_div = document.getElementById("chart_master_div");
    var axisLabels = chart_div.getElementsByTagName('text');
    for (var i = 0; i < axisLabels.length; i++) {
        if (axisLabels[i].getAttribute('text-anchor') === 'end') {
            // what happens with GoogleCharts is that the best vAxis format we can choose is "short"
            // which formats large numbers with "k", "M" and... "B" unit (for "billions"). 
            // However we prefer the "G" unit for billions, so we hack the generated HTML:
            axisLabels[i].innerHTML = axisLabels[i].innerHTML.replace("B", "G")
        }
    }
}

"""
        )

        # convert into JS all the charts that belong to this HTML document:
        combo_box_ctrls = set()
        for num, graph in enumerate(self.graphs, start=1):
            self.file.write(graph.toGoogleChartJS())
            if len(graph.combobox_label) > 0:
                combo_box_ctrls.add(graph.combobox_label)
            if verbose:
                print("Successfully generated JS code for chart with title [%s]." % graph.graph_title)

        if verbose:
            print("Generated JS code for a total of %d charts." % len(self.graphs))

        # add all event listeners for button clicks:
        self.file.write("function on_load_callback() {\n")
        for num, graph in enumerate(self.graphs, start=1):
            if len(graph.combobox_label) == 0:
                self.file.write('  document.getElementById("btn_draw_%s").addEventListener("click", draw_%s);\n' % (graph.js_name, graph.js_name))
            else:
                # this will be selected from a combobox, no need to hook into a button
                pass
        self.file.write('  document.getElementById("btn_show_config").addEventListener("click", show_config_details);\n')
        self.file.write('  set_hidden("config_viewer_div");\n')
        self.file.write("}\n")  # end of on_load_callback()

        # add function to reset all comboboxes
        self.file.write(
            """
/* Utility function used to reset combobox controls: */
function reset_combo_boxes(combobox_to_exclude_from_reset) {
"""
        )
        for num, comboname in enumerate(sorted(combo_box_ctrls), start=1):
            self.file.write('  if (combobox_to_exclude_from_reset != "%s")\n' % comboname)
            self.file.write('      document.getElementById("select_combobox_%s").value = "clear_chart";\n' % comboname)
        self.file.write("}\n")  # end of reset_combo_boxes()

        # add functions to manage visibility of a <DIV>
        self.file.write("\nfunction toggle_visibility(div_name) {\n")
        self.file.write("    var x = document.getElementById(div_name);\n")
        self.file.write('    if (x.style.display === "none")\n')
        self.file.write('       x.style.display = "block";\n')
        self.file.write("    else\n")
        self.file.write('       x.style.display = "none";\n')
        self.file.write("}\n")
        self.file.write("\nfunction set_visible(div_name) {\n")
        self.file.write('    document.getElementById(div_name).style.display = "block";\n')
        self.file.write("}\n")
        self.file.write("\nfunction set_hidden(div_name) {\n")
        self.file.write('    document.getElementById(div_name).style.display = "none";\n')
        self.file.write("}\n")

        # add function that toggles config details
        self.file.write("\nfunction show_config_details() {\n")
        self.file.write('    toggle_visibility("config_viewer_div");\n')
        self.file.write('    toggle_visibility("chart_master_div");\n')
        self.file.write("}\n\n")

        # add function that enforces visibility of currently-selected char
        self.file.write("\nfunction set_main_chart_div_as_visible() {\n")
        self.file.write('    set_hidden("config_viewer_div");\n')
        self.file.write('    set_visible("chart_master_div");\n')
        self.file.write("}\n")

        # close <HEAD> tag
        self.file.write("  </script>\n")
        self.file.write("</head>\n")

    def startHtmlBody(self, cgroup_name, monitored_system, jheader, collected_threads):
        self.file.write("<body>\n")
        self.file.write('  <h1>Data collected from <span id="monitored_system_span">' + monitored_system + "</span></h1>\n")
        self.file.write('  <div id="button_div">\n')
        self.file.write('  <table id="button_table" summary="buttons to visualize charts">\n')

        # Table header row
        self.file.write("  <tr>\n")
        self.file.write('    <td class="button_table_col">Static Info</td>\n')
        self.file.write('    <td class="button_table_col"><b>CGroup</b> stats (Data collected from %s)</td>\n' % cgroup_name)
        if collected_threads:
            self.file.write('    <td class="button_table_col"><b>CGroup</b> per-thread stats (Data collected from cgroup and /proc)</td>\n')
        else:
            self.file.write('    <td class="button_table_col"><b>CGroup</b> per-process stats (Data collected from cgroup and /proc)</td>\n')
        self.file.write('    <td class="button_table_col"><b>Baremetal</b> stats (Data collected only from /proc)</td>\n')
        self.file.write("  </tr>\n")

        # Datarow
        self.file.write("  <tr>\n")
        self.file.write('  <td class="button_table_col">\n')
        self.file.write('    <button id="btn_show_config"><b>Configuration</b></button><br/>\n')
        self.file.write('  </td><td class="button_table_col">\n')

        def write_buttons_for_graph_type(source_data):
            nwritten_controls = 0

            # find all graphs that will be activated through a combobox
            graphs_combobox = {}
            for num, graph in enumerate(self.graphs, start=1):
                if graph.source_data == source_data and len(graph.combobox_label) > 0:
                    if graph.combobox_label not in graphs_combobox:
                        # add new dict entry as empty list
                        graphs_combobox[graph.combobox_label] = []

                    # add to the existing dict entry a new graph:
                    graphs_combobox[graph.combobox_label].append([graph.combobox_entry, graph.js_name])

            # generate the CPU select box:
            if len(graphs_combobox) > 0:
                for combobox_label in graphs_combobox.keys():
                    graph_list = graphs_combobox[combobox_label]
                    self.file.write('    <select id="select_combobox_%s" onchange="call_function_named(this.value)">\n' % (combobox_label))
                    self.file.write('      <option value="clear_chart">None</option>\n')
                    for entry in graph_list:
                        button_label = entry[0]
                        js_name = entry[1]
                        self.file.write('      <option value="draw_%s">%s</option>\n' % (js_name, button_label))
                    self.file.write("    </select>\n")
                    nwritten_controls += 1

            # find in all graphs registered so far all those related to the CGROUP
            for num, graph in enumerate(self.graphs, start=1):
                if graph.source_data == source_data:
                    if len(graph.combobox_label) > 0:
                        continue  # skip - already drawn via <select>
                    elif "CPU" in graph.button_label:
                        colour = "red"
                    elif graph.button_label.startswith("Memory"):
                        colour = "darkorange"
                    elif graph.button_label.startswith("Network"):
                        colour = "darkblue"
                    elif graph.button_label.startswith("Disk"):
                        colour = "darkgreen"
                    else:
                        colour = "black"
                    self.file.write(
                        '    <button id="btn_draw_' + graph.js_name + '" style="color:' + colour + '"><b>' + graph.button_label + "</b></button>\n"
                    )
                    nwritten_controls += 1

            if nwritten_controls == 0:
                self.file.write("N/A")

        write_buttons_for_graph_type(GRAPH_SOURCE_DATA_IS_CGROUP_TOTALS)
        self.file.write('      </td><td class="button_table_col">\n')
        write_buttons_for_graph_type(GRAPH_SOURCE_DATA_IS_CGROUP_PER_PROCESS)
        self.file.write('      </td><td class="button_table_col">\n')
        write_buttons_for_graph_type(GRAPH_SOURCE_DATA_IS_BAREMETAL)

        self.file.write("  </td></tr>\n")
        self.file.write("  </table>\n")
        self.file.write("  </div>\n")
        # self.file.write("  <p></p>\n")

        # finally generate the MAIN div: i.e. where the selected chart is going to be drawn:
        self.file.write(
            '  <div id="chart_master_div"><div id="chart_master_inner_div"><p id="chart_master_inner_p">...click on a button above to show a graph...</p></div></div>\n'
        )

        def configdump(jheader, section, displayName):
            # newstr = '<h3>' + displayName + '</h3>\n'
            newstr = "    <tr><td colspan='2' class='config_sectioncol'>" + displayName + "</td></tr>\n"
            config_dict = jheader[section]
            for label in config_dict:
                newstr += "    <tr><td class='config_key'>%s</td><td class='config_val'>%s</td></tr>\n" % (
                    label.capitalize().replace("_", " "),
                    str(config_dict[label]),
                )
            return newstr

        def aggregate_cpuinfo(jheader):
            cpudict = {}

            # first take the unique strings about the CPU vendor/model
            for field_name in ["vendor_id", "model_name"]:
                cpudict[field_name] = set()
                for cpu_name in jheader["cpuinfo"].keys():
                    cpudict[field_name].add(jheader["cpuinfo"][cpu_name][field_name])

            # secondly take the unique values of min/max frequency, MIPS, cache size
            for field_name in ["scaling_min_freq_mhz", "scaling_max_freq_mhz", "bogomips", "cache_size_kb"]:
                cpudict[field_name] = set()
                for cpu_name in jheader["cpuinfo"].keys():
                    cpuinfo_from_header = jheader["cpuinfo"][cpu_name]
                    if field_name in cpuinfo_from_header:  # these fields are optionals: cmonitor_collector may not be able to populate them
                        cpudict[field_name].add(int(cpuinfo_from_header[field_name]))

            # now convert each dictionary entry from a set() to a simple string:
            for field_name in cpudict.keys():
                the_list = [str(v) for v in cpudict[field_name]]
                # join by comma each set() inside the dict:
                if len(the_list) > 0:
                    cpudict[field_name] = ",".join(the_list)
                else:
                    cpudict[field_name] = "Not Available"

            return cpudict

        # immediately after the MAIN div, the element where the configuration info are shown (when toggled):
        self.file.write('  <div id="config_viewer_div">\n')
        self.file.write("  <h2>Monitored System Details</h2>\n")
        self.file.write("  <table summary='details on monitored system'>\n")
        self.file.write(configdump(jheader, "identity", "Server Identity"))
        self.file.write(configdump(jheader, "os_release", "Operating System Release"))
        self.file.write(configdump(jheader, "proc_version", "Linux Kernel Version"))
        if "cgroup_config" in jheader:  # if cgroups are off, this section will not be present
            self.file.write(configdump(jheader, "cgroup_config", "Linux Control Group (CGroup) Configuration"))
        if "cpuinfo" in jheader:
            jheader["cpu_summary"] = aggregate_cpuinfo(jheader)
            self.file.write(configdump(jheader, "cpu_summary", "CPU Overview"))
        if "numa_nodes" in jheader:
            self.file.write(configdump(jheader, "numa_nodes", "NUMA Overview"))
        if "proc_meminfo" in jheader:
            self.file.write(configdump(jheader, "proc_meminfo", "Memory Overview"))
        # self.file.write(configdump(jheader, "cpuinfo", "CPU Core Details")
        self.file.write("  </table>\n")
        self.file.write("  <h2>CMonitor Collector</h2>\n")
        self.file.write("  <table summary='details on cmonitor_collector'>\n")
        self.file.write(configdump(jheader, "cmonitor", "Performance Stats Collector Configuration"))
        if "custom_metadata" in jheader:
            if len(jheader["custom_metadata"]) > 0:
                self.file.write(configdump(jheader, "custom_metadata", "Custom Metadata"))
        self.file.write("  </table>\n")
        self.file.write("  </div>\n")  # end of 'config_viewer_div'

    def appendHtmlTable(self, name, table_entries, div_class="bottom_div"):
        self.file.write("  <div class='" + div_class + "'>\n")
        self.file.write("    <h3>" + name + "</h3>\n")
        self.file.write("    <table summary='textbox with summarized info'>\n")
        self.file.write("    <tr><td><ul>\n")
        for i, entry in enumerate(table_entries, start=1):
            self.file.write("      <li>" + entry[0] + " <span class='bottom_table_val'>" + entry[1] + "</span></li>\n")
            if (i % 4) == 0 and i < len(table_entries):
                self.file.write("      </ul></td><td><ul>\n")
        self.file.write("    </ul></td></tr>\n")
        self.file.write("    </table>\n")
        self.file.write("  </div>\n")

    def endHtmlBody(self):
        self.file.write("</body>\n")
        self.file.write("</html>\n")
        self.file.close()


# =======================================================================================================
# CMonitorGraphGenerator
# =======================================================================================================


class CMonitorGraphGenerator:
    """
    This is the main class of cmonitor_chart, able to read a JSON file produced by cmonitor_collector,
    extract the most useful information and render them inside an HtmlOutputPage object.
    """

    def __init__(self, outfile, jheader, jdata):
        self.jheader = jheader  # a dictionary with cmonitor_collector "header" JSON object
        self.jdata = jdata  # a list of dictionaries with cmonitor_collector "samples" objects

        # in many places below we need to get "immutable" data that we know won't change across all samples
        # like the names of network devices or the list of CPUs...
        # since for some metrics the very sample does not contain any KPI (e.g. cgroup network traffic is generated
        # only for samples after the first one) if possible we pick the 2nd sample and not the 1st one:
        assert len(self.jdata) >= 2
        self.sample_template = self.jdata[1]

        # did we collect at PROCESS-level granularity or just at THREAD-level granularity?
        string_collected_kpis = self.jheader["cmonitor"]["collecting"]  # e.g. "cgroup_cpu,cgroup_memory,cgroup_threads"
        self.collected_threads = "cgroup_threads" in string_collected_kpis
        if verbose:
            if self.collected_threads:
                print("Per-thread stats (instead of per-process stats) have been collected in the input JSON file.")
            else:
                print("Per-process stats (instead of per-thread stats) have been collected in the input JSON file.")

        # detect num of CPUs:
        self.baremetal_logical_cpus_indexes = []
        if "stat" in self.sample_template:
            self.baremetal_logical_cpus_indexes = CMonitorGraphGenerator.collect_logical_cpu_indexes_from_section(self.sample_template, "stat")
            if verbose:
                print(
                    "Found %d CPUs in baremetal stats with logical indexes [%s]"
                    % (
                        len(self.baremetal_logical_cpus_indexes),
                        ", ".join(str(x) for x in self.baremetal_logical_cpus_indexes),
                    )
                )

        self.cgroup_logical_cpus_indexes = []
        if "cgroup_cpuacct_stats" in self.sample_template:
            self.cgroup_logical_cpus_indexes = CMonitorGraphGenerator.collect_logical_cpu_indexes_from_section(
                self.sample_template, "cgroup_cpuacct_stats"
            )
            if verbose:
                print(
                    "Found %d CPUs in cgroup stats with logical indexes [%s]"
                    % (
                        len(self.cgroup_logical_cpus_indexes),
                        ", ".join(str(x) for x in self.cgroup_logical_cpus_indexes),
                    )
                )

        # load IDENTITY of monitored system
        self.monitored_system = "Unknown"
        if "identity" in self.jheader:
            if "hostname" in self.jheader["identity"]:
                self.monitored_system = self.jheader["identity"]["hostname"]
        if "custom_metadata" in self.jheader:
            if "cmonitor_chart_name" in self.jheader["custom_metadata"]:
                self.monitored_system = self.jheader["custom_metadata"]["cmonitor_chart_name"]

        # get the CGROUP name
        self.cgroup_name = "None"
        if "cgroup_config" in self.jheader and "name" in self.jheader["cgroup_config"]:
            self.cgroup_name = self.jheader["cgroup_config"]["name"]
        if "custom_metadata" in self.jheader:
            if "cmonitor_chart_name" in self.jheader["custom_metadata"]:
                self.cgroup_name = "docker/" + self.jheader["custom_metadata"]["cmonitor_chart_name"]

        # get the CGROUP version (v1 or v2 ?)
        self.cgroup_ver = None
        if "cgroup_config" in self.jheader and "version" in self.jheader["cgroup_config"]:
            self.cgroup_ver = int(self.jheader["cgroup_config"]["version"])

        # finally create the main HTML output page object
        self.output_page = HtmlOutputPage(outfile, self.monitored_system)

    # =======================================================================================================
    # Private helpers
    # =======================================================================================================

    @staticmethod
    def collect_logical_cpu_indexes_from_section(jsample, section_name):
        """
        Walks over given JSON sample looking for keys 'cpuXYZ' and storing all 'XYZ' CPU indexes.
        Returns a list of CPU indexes
        """
        logical_cpus_indexes = []
        for key in jsample[section_name]:
            if key.startswith("cpu") and key != "cpu_total" and key != "cpu_tot":
                cpuIdx = int(key[3:])
                logical_cpus_indexes.append(cpuIdx)
                # print("%s %s" %(key, cpuIdx))
        return logical_cpus_indexes

    @staticmethod
    def sizeof_fmt(num, suffix="B"):
        for unit in ["", "k", "M", "G", "T", "P", "E", "Z"]:
            if abs(num) < 1000.0:
                return "%3.1f%s%s" % (num, unit, suffix)
            num /= 1000.0
        return "%.1f%s%s" % (num, "Y", suffix)

    def __make_jheader_nicer(self):
        """
        This function just improves self.jheader by adding new sections in that dict and
        adding measurement units where they are required.
        This is useful because the
        """

        # provide some human-readable config files:
        if "cgroup_config" in self.jheader:
            avail_cpus = self.jheader["cgroup_config"]["cpus"].split(",")
            self.jheader["cgroup_config"]["num_allowed_cpus"] = len(avail_cpus)
            self.jheader["cgroup_config"]["cpus"] = self.jheader["cgroup_config"]["cpus"].replace(",", ", ")

            self.jheader["cgroup_config"]["memory_limit_bytes"] = self.__cgroup_get_memory_limit_human_friendly()
            self.jheader["cgroup_config"]["cpu_quota_perc"] = self.__cgroup_get_cpu_quota_human_friendly()

        if "cmonitor" in self.jheader:
            if self.jheader["cmonitor"]["sample_num"] == 0:
                self.jheader["cmonitor"]["sample_num"] = "Infinite"

        if "proc_meminfo" in self.jheader:
            self.jheader["proc_meminfo"]["MemTotal"] = CMonitorGraphGenerator.sizeof_fmt(int(self.jheader["proc_meminfo"]["MemTotal"]))
            self.jheader["proc_meminfo"]["Hugepagesize"] = CMonitorGraphGenerator.sizeof_fmt(int(self.jheader["proc_meminfo"]["Hugepagesize"]))

    def __print_data_loading_stats(self, desc, n_invalid_samples):
        if n_invalid_samples > 0:
            print(
                "While parsing %s statistics found %d/%d (%.1f%%) samples that did not contain some required JSON section."
                % (
                    desc,
                    n_invalid_samples,
                    len(self.jdata),
                    100 * n_invalid_samples / len(self.jdata),
                )
            )
        else:
            print("Parsed correctly %d samples for [%s] category" % (len(self.jdata), desc))

    def __cgroup_get_cpu_quota_percentage(self):
        """
        Returns a number, in percentage, that indicates how much&many CPUs can be used.
        E.g. possible values are 50%, 140%, 300% or -1 to indicate no limit.
        """
        cpu_quota_perc = 100
        if "cpu_quota_perc" in self.jheader["cgroup_config"]:
            cpu_quota_perc = 100 * self.jheader["cgroup_config"]["cpu_quota_perc"]
            if cpu_quota_perc == -100:  # means there's no CPU limit
                cpu_quota_perc = -1
        return cpu_quota_perc

    def __cgroup_get_cpu_quota_human_friendly(self):
        if "cpu_quota_perc" not in self.jheader["cgroup_config"]:
            return "NO LIMIT"
        if int(self.jheader["cgroup_config"]["cpu_quota_perc"]) == -1:
            return "NO LIMIT"
        cpu_quota_perc = 100 * self.jheader["cgroup_config"]["cpu_quota_perc"]
        return f"cpu quota = {cpu_quota_perc}%"

    @staticmethod
    def cgroup_get_cpu_throttling(s):
        cpu_throttling = 0
        if "throttling" in s["cgroup_cpuacct_stats"]:
            # throttling is new since cmonitor_collector 1.5-0
            nr_periods = s["cgroup_cpuacct_stats"]["throttling"]["nr_periods"]
            if nr_periods:
                cpu_throttling = 100 * s["cgroup_cpuacct_stats"]["throttling"]["nr_throttled"] / nr_periods
        return cpu_throttling

    def __cgroup_get_memory_limit(self):
        """
        Returns the cgroup memory limit in bytes; can be -1 if there's no limit
        """
        cgroup_limit_bytes = -1
        if "memory_limit_bytes" in self.jheader["cgroup_config"]:
            # IMPORTANT: this value could be -1 if there's no limit
            cgroup_limit_bytes = int(self.jheader["cgroup_config"]["memory_limit_bytes"])

        return cgroup_limit_bytes

    def __cgroup_get_memory_limit_human_friendly(self):
        if "memory_limit_bytes" not in self.jheader["cgroup_config"]:
            return "NO LIMIT"
        if int(self.jheader["cgroup_config"]["memory_limit_bytes"]) == -1:
            return "NO LIMIT"
        cgroup_limit_bytes = CMonitorGraphGenerator.sizeof_fmt(self.jheader["cgroup_config"]["memory_limit_bytes"])
        return f"memory limit = {cgroup_limit_bytes}"

    @staticmethod
    def __get_main_thread_associated_with(sample, tid):
        json_key = "pid_%s" % tid
        tgid = sample["cgroup_tasks"][json_key]["tgid"]
        if tgid == sample["cgroup_tasks"][json_key]["pid"]:
            # actually current entry is not a secondary thread but a PROCESS, append it:
            return tid
        else:
            json_key_of_main_process = "pid_%s" % tgid
            if json_key_of_main_process in sample["cgroup_tasks"]:
                return tgid
            else:
                # the main thread / process associated with given THREAD ID is missing:
                return None

    def __get_main_threads_only(self, tids_list_to_filter):
        x = set()  # use a Python set to automatically remove duplicates
        for tid in tids_list_to_filter:
            json_key = "pid_%s" % tid

            # first of all, find the first JSON sample that contains the current TID
            n_sample = 0
            while n_sample < len(self.jdata) and json_key not in self.jdata[n_sample]["cgroup_tasks"]:
                n_sample += 1

            assert n_sample < len(self.jdata)  # the TID comes from a processing of self.jdata itself... it must be there
            pid = CMonitorGraphGenerator.__get_main_thread_associated_with(self.jdata[n_sample], tid)
            if pid is None:
                print(f"WARNING: the input JSON does not contain collected stats for PID [{pid}] associated with thread ID [{tid}]...")
            else:
                x.add(pid)

        return x

    def __generate_topN_procs_bubble_chart(self, process_dict, topN_pids_list, max_byte_value_dict, chart_desc_postfix):
        cpu_label = "CPU time"
        io_label = "I/O (B)"
        thread_proc_label = "Thread" if self.collected_threads else "Process"
        memory_label = "Memory (B)"

        def get_nice_process_or_thread_name(pid):
            return "%s (%d)" % (process_dict[pid]["cmd"], pid)

        # now select the N top processes and put their data in a GoogleChart table:
        topN_process_table = GoogleChartsGenericTable(["Command", cpu_label, io_label, thread_proc_label, memory_label])
        for i, pid in enumerate(topN_pids_list):
            p = process_dict[pid]
            nicecmd = get_nice_process_or_thread_name(pid)
            if verbose:
                print("Processing data for %d-th CPU-top-scorer process [%s]" % (i + 1, nicecmd))
            topN_process_table.addRow([p["cmd"], p["cpu"], int(p["io"]), nicecmd, int(p["mem"])])

        # generate the bubble chart graph:
        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=topN_process_table,
                button_label="CPU/Memory/Disk Bubbles by Thread" if self.collected_threads else "CPU/Memory/Disk Bubbles by Process",
                graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_PER_PROCESS,
                graph_type=GRAPH_TYPE_BUBBLE_CHART,
                graph_title=f"CPU/disk total usage on X/Y axes; memory usage as bubble size {chart_desc_postfix}",
                y_axes_titles=[cpu_label, io_label],
            )
        )

    def __generate_topN_procs_cpu_io_mem_vs_time(self, process_dict, topN_pids_list, max_byte_value_dict, chart_desc_postfix):
        mem_limit_bytes = self.__cgroup_get_memory_limit()
        cpu_quota_perc = self.__cgroup_get_cpu_quota_percentage()

        def get_nice_process_or_thread_name(pid):
            return "%s (%d)" % (process_dict[pid]["cmd"], pid)

        chart_data = {}

        ## -- CPU --
        if cpu_quota_perc > 0:
            # it is possible to compute the "idle" time inside this cgroup and it's possible that CPU is throttled...
            # so in such case we add 2 more data series to the chart:
            cpu_time_serie = GoogleChartsTimeSeries(
                ["Timestamp"] + [get_nice_process_or_thread_name(pid) for pid in topN_pids_list] + ["Idle", "Throttling"],
                [""] + ["%" for pid in topN_pids_list] + ["%", "%"],
            )
            y_axes_max_value = [None, 0]
            columns_for_2nd_yaxis = ["Throttling"]
            y_axes_titles = ["CPU (%)", "CPU Throttling (%)"]
        else:
            # no CPU limit... creating an "idle" column (considering as max CPU all the CPUs available) likely produces weird
            # results out-of-scale (imagine servers with hundreds of CPUs and cmonitor_collector monitoring just a Redis container!)
            # so we do not place any "idle" column. The "throttling" column does not apply either.
            assert cpu_quota_perc == -1
            cpu_time_serie = GoogleChartsTimeSeries(
                ["Timestamp"] + [get_nice_process_or_thread_name(pid) for pid in topN_pids_list],
                [""] + ["%" for pid in topN_pids_list],
            )
            y_axes_max_value = [None]  # let Google Charts autocompute the Y axes limits
            columns_for_2nd_yaxis = None
            y_axes_titles = ["CPU (%)"]

        # CPU by thread/process:
        chart_data["cpu"] = GoogleChartsGraph(
            data=cpu_time_serie,
            graph_title=f"CPU usage ({self.__cgroup_get_cpu_quota_human_friendly()}) {chart_desc_postfix}",
            button_label="CPU by Thread" if self.collected_threads else "CPU by Process",
            graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_PER_PROCESS,
            # take any contribute of any thread/process and stack it together: this way it becomes easier to spot
            # when the cgroup CPU limit was hit and due to which threads/processes
            stack_state=True,
            y_axes_titles=y_axes_titles,
            # throttling should not be stacked to the CPU usage contributions, so move it on 2nd y axis:
            columns_for_2nd_yaxis=columns_for_2nd_yaxis,
            # make the 2 axes have the same identical Y scale to make it easier to read it:
            y_axes_max_value=y_axes_max_value,
        )

        ## -- MEM --
        topN_pids_list_for_memory = self.__get_main_threads_only(topN_pids_list)
        if verbose:
            print(
                f"While generating the per-process memory chart, the following MAIN PIDs were selected: {topN_pids_list_for_memory} from the list of top-CPU scorer processes {topN_pids_list}"
            )
        if mem_limit_bytes > 0:
            # it is possible to compute the "free" memory inside this cgroup and it's possible to have allocation failures
            # so in such case we add 2 more data series to the chart:
            mem_time_serie = GoogleChartsTimeSeries(
                ["Timestamp"] + [get_nice_process_or_thread_name(pid) for pid in topN_pids_list_for_memory] + ["Free", "Alloc Failures"],
                [""] + ["B" for pid in topN_pids_list_for_memory] + ["B", ""],
            )
            y_axes_max_value = [None, 0]
            columns_for_2nd_yaxis = ["Alloc Failures"]
            y_axes_titles = ["Memory (B)", "Alloc Failures"]
        else:
            # no memory limit... creating the "free" data serie (considering all the system memory as actual limit) likely produces weird
            # results out-of-scale so we do not place any "idle" column. The "alloc failures" column does not apply either.
            assert mem_limit_bytes == -1
            mem_time_serie = GoogleChartsTimeSeries(
                ["Timestamp"] + [get_nice_process_or_thread_name(pid) for pid in topN_pids_list_for_memory],
                [""] + ["B" for pid in topN_pids_list_for_memory],
            )
            y_axes_max_value = [None]  # let Google Charts autocompute the Y axes limits
            columns_for_2nd_yaxis = None
            y_axes_titles = ["Memory (B)"]

        chart_data["mem"] = GoogleChartsGraph(
            data=mem_time_serie,
            graph_title=f"Memory usage ({self.__cgroup_get_memory_limit_human_friendly()}) {chart_desc_postfix}",
            button_label="Memory by Process",
            graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_PER_PROCESS,
            # take any contribute of any thread/process and stack it together: this way it becomes easier to spot
            # when the cgroup MEMORY limit was hit and due to which threads/processes
            stack_state=True,
            y_axes_titles=y_axes_titles,
            y_axes_max_value=y_axes_max_value,
            # alloc failures should not be stacked to the memory usage contributions, so move it on 2nd y axis:
            columns_for_2nd_yaxis=columns_for_2nd_yaxis,
        )

        ## -- IO --
        io_time_serie = GoogleChartsTimeSeries(
            ["Timestamp"] + [get_nice_process_or_thread_name(pid) for pid in topN_pids_list],
            [""] + ["B" for pid in topN_pids_list],
        )
        chart_data["io"] = GoogleChartsGraph(
            data=io_time_serie,
            graph_title=f"I/O usage (from cgroup stats) {chart_desc_postfix}",
            button_label="IO by Thread" if self.collected_threads else "IO by Process",
            y_axes_titles=["IO Read+Write (B)"],
            graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_PER_PROCESS,
            stack_state=False,
        )

        # now generate a table of CPU/IO/MEMORY usage over time, per process/thread:
        for sample in self.jdata:
            try:
                row = {}

                # save the same timestamp in all charts
                for key in ["cpu", "io", "mem"]:
                    row[key] = [sample["timestamp"]["UTC"]]

                # append CPU & IO samples
                tot_cpu_usage_perc = 0
                for top_process_pid in topN_pids_list:
                    # print(top_process_pid)
                    json_key = "pid_%s" % top_process_pid
                    if json_key in sample["cgroup_tasks"]:
                        top_proc_sample = sample["cgroup_tasks"][json_key]

                        cpu = top_proc_sample["cpu_usr"] + top_proc_sample["cpu_sys"]
                        io = int((top_proc_sample["io_rchar"] + top_proc_sample["io_wchar"]))

                        tot_cpu_usage_perc += cpu
                        row["cpu"].append(cpu)
                        row["io"].append(io)
                    else:
                        # probably this process was born later or dead earlier than this timestamp
                        row["cpu"].append(0)
                        row["io"].append(0)

                # for memory chart, only include PROCESSES, never include SECONDARY THREADS since there
                # is no distinction between memory of whole process and memory of secondary threads
                tot_mem_usage_bytes = 0
                for top_process_pid in topN_pids_list_for_memory:
                    # print(top_process_pid)
                    json_key = "pid_%s" % top_process_pid
                    if json_key in sample["cgroup_tasks"]:
                        top_proc_sample = sample["cgroup_tasks"][json_key]

                        tot_mem_usage_bytes += top_proc_sample["mem_rss_bytes"]
                        mem = int(top_proc_sample["mem_rss_bytes"])
                        row["mem"].append(mem)
                    else:
                        # probably this process was born later or dead earlier than this timestamp
                        row["mem"].append(0)

                # CPU graph has
                # - idle (if cpu_quota_perc > 0)
                # - throttling
                # as additional columns right after the last PID serie
                if cpu_quota_perc > 0:
                    row["cpu"].append(max(cpu_quota_perc - tot_cpu_usage_perc, 0))
                    row["cpu"].append(CMonitorGraphGenerator.cgroup_get_cpu_throttling(sample))

                # Memory graph has
                # - free mem
                # - alloc failures
                # as additional columns right after the timestamp
                if mem_limit_bytes > 0:
                    row["mem"].append(max(int(mem_limit_bytes - tot_mem_usage_bytes), 0))
                    if self.cgroup_ver == 1:
                        failcnt = sample["cgroup_memory_stats"]["events.failcnt"]
                    else:
                        failcnt = sample["cgroup_memory_stats"]["events.oom_kill"]
                    row["mem"].append(failcnt)

                for key in ["cpu", "io", "mem"]:
                    chart_data[key].data_table.addRow(row[key])
            except KeyError:  # avoid crashing if a key is not present in the dictionary...
                # print("Missing cgroup data while parsing sample %d" % i)
                pass

        self.output_page.appendGoogleChart(chart_data["cpu"])
        self.output_page.appendGoogleChart(chart_data["mem"])
        self.output_page.appendGoogleChart(chart_data["io"])

    # =======================================================================================================
    # Public API
    # =======================================================================================================

    def generate_cgroup_topN_procs(self, numProcsToShow=20, thread_filter=""):
        # if process data was not collected, just return:
        if "cgroup_tasks" not in self.sample_template:
            return

        # build a dictionary containing cumulative metrics for CPU/IO/MEM data for each process
        # along all collected samples
        process_dict = {}
        max_byte_value_dict = {}
        max_byte_value_dict["mem_rss"] = 0
        max_byte_value_dict["io_total"] = 0
        n_invalid_samples = 0
        for i, sample in enumerate(self.jdata):
            try:
                for process in sample["cgroup_tasks"]:
                    # parse data from JSON
                    entry = sample["cgroup_tasks"][process]
                    cmd = entry["cmd"]
                    # filter by thread name if there is a filter
                    if thread_filter and thread_filter not in cmd:
                        continue
                    cputime = entry["cpu_usr_total_secs"] + entry["cpu_sys_total_secs"]
                    iobytes = entry["io_total_read"] + entry["io_total_write"]
                    membytes = entry["mem_rss_bytes"]  # take RSS, more realistic/useful compared to the "mem_virtual_bytes"
                    thepid = entry["pid"]  # can be the TID (thread ID) if cmonitor_collector was started with --collect=cgroup_threads

                    # keep track of maxs:
                    max_byte_value_dict["mem_rss"] = max(membytes, max_byte_value_dict["mem_rss"])
                    max_byte_value_dict["io_total"] = max(iobytes, max_byte_value_dict["io_total"])

                    try:  # update the current entry
                        process_dict[thepid]["cpu"] = cputime
                        process_dict[thepid]["io"] = iobytes
                        process_dict[thepid]["mem"] = membytes
                        process_dict[thepid]["cmd"] = cmd

                        # FIXME FIXME
                        # process_dict[thepid]["is_thread"] =
                    except:  # no current entry so add one
                        process_dict.update(
                            {
                                thepid: {
                                    "cpu": cputime,
                                    "io": iobytes,
                                    "mem": membytes,
                                    "cmd": cmd,
                                }
                            }
                        )
            except KeyError as e:  # avoid crashing if a key is not present in the dictionary...
                print(f"Missing cgroup data while parsing {i}-th sample: {e}")
                n_invalid_samples += 1
                pass

        self.__print_data_loading_stats("per-process", n_invalid_samples)

        # now sort all collected processes by the amount of CPU*memory used:
        # NOTE: sorted() will return just the sorted list of KEYs = PIDs
        def sort_key(d):
            # return process_dict[d]['cpu'] * process_dict[d]['mem']
            return process_dict[d]["cpu"]

        topN_pids_list = sorted(process_dict, key=sort_key, reverse=True)

        # truncate to first N:
        if numProcsToShow > 0:
            topN_pids_list = topN_pids_list[0:numProcsToShow]

        # provide common chart description
        chart_desc_postfix = ""
        if numProcsToShow > 0:
            if self.collected_threads:
                chart_desc_postfix = f"of {numProcsToShow} top-CPU-utilizing threads"
            else:
                chart_desc_postfix = f"of {numProcsToShow} top-CPU-utilizing processes"
        # else: if there's no filter on the processes to show, simply produce an empty postfix since it's
        #      weird to see e.g. "CPU usage of ALL top-CPU-utilizing processes"

        self.__generate_topN_procs_cpu_io_mem_vs_time(process_dict, topN_pids_list, max_byte_value_dict, chart_desc_postfix)
        self.__generate_topN_procs_bubble_chart(process_dict, topN_pids_list, max_byte_value_dict, chart_desc_postfix)

    def generate_baremetal_disks_io(self):
        # if disk data was not collected, just return:
        if "disks" not in self.sample_template:
            return

        all_disks = self.sample_template["disks"].keys()
        if len(all_disks) == 0:
            return

        # see https://www.kernel.org/doc/Documentation/iostats.txt

        diskcols = ["Timestamp"]
        for device in all_disks:
            # diskcols.append(str(device) + " Disk Time")
            # diskcols.append(str(device) + " Reads")
            # diskcols.append(str(device) + " Writes")
            diskcols.append(str(device) + " Read MB")
            diskcols.append(str(device) + " Write MB")

        # convert from kB to MB
        divider = 1000

        #
        # MAIN LOOP
        # Process JSON sample and fill the GoogleChartsTimeSeries() object
        #

        disk_table = GoogleChartsTimeSeries(diskcols)
        for i, s in enumerate(self.jdata):
            if i == 0:
                continue

            row = []
            row.append(s["timestamp"]["UTC"])
            for device in all_disks:
                # row.append(s["disks"][device]["time"])
                # row.append(s["disks"][device]["reads"])
                # row.append(s["disks"][device]["writes"])
                row.append(s["disks"][device]["rkb"] / divider)
                row.append(-s["disks"][device]["wkb"] / divider)
            disk_table.addRow(row)

        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=disk_table,
                button_label="Disk I/O",
                graph_source=GRAPH_SOURCE_DATA_IS_BAREMETAL,
                graph_title="Disk I/O (from baremetal stats)",
                y_axes_titles=["MB"],
            )
        )
        return

    # def generate_filesystems(self.output_page, self.jdata):
    #     global self.graphs
    #     fsstr = ""
    #     for fs in self.sample_template["filesystems"].keys():
    #         fsstr = fsstr + "'" + fs + "',"
    #     fsstr = fsstr[:-1]
    #     writeHtmlHead_line_graph(self.output_page, fsstr)
    #     for i, s in enumerate(self.jdata):
    #         self.output_page.write(",['Date(%s)' " % (googledate(s['timestamp']["UTC"])))
    #         for fs in s["filesystems"].keys():
    #             self.output_page.write(", %.1f" % (s["filesystems"][fs]["fs_full_percent"]))
    #         self.output_page.write("]\n")
    #     self.output_page.appendGoogleChart(GoogleChartsGraph( 'File Systems Used percent')
    #     return

    def __generate_network_traffic_graphs(self, graph_source, sample_section_name, graph_desc):
        # if network traffic data was not collected, just return:
        if sample_section_name not in self.sample_template:
            return

        all_netdevices = self.sample_template[sample_section_name].keys()
        if len(all_netdevices) == 0:
            return

        netcols = ["Timestamp"]
        for device in all_netdevices:
            netcols.append(str(device) + "+in")
            netcols.append(str(device) + "-out")

        # convert from bytes to MB
        divider = 1000 * 1000
        unit = "MB"

        #
        # MAIN LOOP
        # Process JSON sample and fill the GoogleChartsTimeSeries() object
        #

        # MB/sec

        net_table = GoogleChartsTimeSeries(netcols, [unit for col in netcols])
        for i, s in enumerate(self.jdata):
            if i == 0:
                continue

            row = [s["timestamp"]["UTC"]]
            for device in all_netdevices:
                try:
                    row.append(+s[sample_section_name][device]["ibytes"] / divider)
                    row.append(-s[sample_section_name][device]["obytes"] / divider)
                except KeyError:
                    if verbose:
                        print("Missing key '%s' while parsing sample %d" % (device, i))
                    row.append(0)
                    row.append(0)
            net_table.addRow(row)

        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=net_table,
                graph_title=f"Network Traffic in MB/s {graph_desc}",
                button_label="Network (MB/s)",
                y_axes_titles=["MB/s"],
                graph_source=graph_source,
                stack_state=False,
            )
        )

        # PPS

        net_table = GoogleChartsTimeSeries(netcols)
        for i, s in enumerate(self.jdata):
            if i == 0:
                continue

            row = [s["timestamp"]["UTC"]]
            for device in all_netdevices:
                try:
                    row.append(+s[sample_section_name][device]["ipackets"])
                    row.append(-s[sample_section_name][device]["opackets"])
                except KeyError:
                    if verbose:
                        print("Missing key '%s' while parsing sample %d" % (device, i))
                    row.append(0)
                    row.append(0)
            net_table.addRow(row)

        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=net_table,
                graph_title=f"Network Traffic in PPS {graph_desc}",
                button_label="Network (PPS)",
                y_axes_titles=["PPS"],
                graph_source=graph_source,
                stack_state=False,
            )
        )
        return

    def generate_baremetal_network_traffic(self):
        self.__generate_network_traffic_graphs(GRAPH_SOURCE_DATA_IS_BAREMETAL, "network_interfaces", "(from baremetal stats)")

    def generate_cgroup_network_traffic(self):
        self.__generate_network_traffic_graphs(GRAPH_SOURCE_DATA_IS_CGROUP_TOTALS, "cgroup_network", "(from cgroup stats)")

    def generate_baremetal_cpus(self):
        # if baremetal CPU data was not collected, just return:
        if "stat" not in self.sample_template:
            return

        # prepare empty tables
        baremetal_cpu_stats = {}
        for c in self.baremetal_logical_cpus_indexes:
            baremetal_cpu_stats[c] = GoogleChartsTimeSeries(
                [
                    "Timestamp",
                    "User",
                    "Nice",
                    "System",
                    "Idle",
                    "I/O wait",
                    "Hard IRQ",
                    "Soft IRQ",
                    "Steal",
                ],
                [
                    "",
                    "%",
                    "%",
                    "%",
                    "%",
                    "%",
                    "%",
                    "%",
                    "%",
                ],
            )

        all_cpus_table = GoogleChartsTimeSeries(
            ["Timestamp"] + [("CPU" + str(x)) for x in self.baremetal_logical_cpus_indexes],  # force newline
            [""] + ["%" for x in self.baremetal_logical_cpus_indexes],
        )

        #
        # MAIN LOOP
        # Process JSON sample and fill the GoogleChartsTimeSeries() object
        #

        for i, s in enumerate(self.jdata):
            if i == 0:
                continue  # skip first sample

            ts = s["timestamp"]["UTC"]
            all_cpus_row = [ts]
            for c in self.baremetal_logical_cpus_indexes:
                cpu_stats = s["stat"]["cpu" + str(c)]
                cpu_total = (
                    cpu_stats["user"]
                    + cpu_stats["nice"]
                    + cpu_stats["sys"]
                    + cpu_stats["iowait"]
                    + cpu_stats["hardirq"]
                    + cpu_stats["softirq"]
                    + cpu_stats["steal"]
                )
                baremetal_cpu_stats[c].addRow(
                    [
                        ts,
                        cpu_stats["user"],
                        cpu_stats["nice"],
                        cpu_stats["sys"],
                        cpu_stats["idle"],
                        cpu_stats["iowait"],
                        cpu_stats["hardirq"],
                        cpu_stats["softirq"],
                        cpu_stats["steal"],
                    ]
                )
                all_cpus_row.append(cpu_total)

            all_cpus_table.addRow(all_cpus_row)

        # Produce the javascript:
        for c in self.baremetal_logical_cpus_indexes:
            self.output_page.appendGoogleChart(
                GoogleChartsGraph(
                    data=baremetal_cpu_stats[c],  # Data
                    graph_title="Logical CPU " + str(c) + " (from baremetal stats)",
                    combobox_label="baremetal_cpus",
                    combobox_entry="CPU" + str(c),
                    y_axes_titles=["CPU (%)"],
                    graph_source=GRAPH_SOURCE_DATA_IS_BAREMETAL,
                    stack_state=True,
                )
            )

        # Also produce the "all CPUs" graph
        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=all_cpus_table,  # Data
                graph_title="All logical CPUs (from baremetal stats)",
                button_label="All CPUs",
                y_axes_titles=["CPU (%)"],
                graph_source=GRAPH_SOURCE_DATA_IS_BAREMETAL,
                stack_state=False,
            )
        )
        return

    def generate_cgroup_cpus(self):
        if "cgroup_cpuacct_stats" not in self.sample_template:
            return  # cgroup mode not enabled at collection time!

        # prepare empty tables
        cpu_stats_table = {}
        for c in self.cgroup_logical_cpus_indexes:
            cpu_stats_table[c] = GoogleChartsTimeSeries(["Timestamp", "User", "System"], ["", "%", "%"])

        all_cpus_table = GoogleChartsTimeSeries(
            ["Timestamp", "Limit/Quota", "Throttling"] + [("CPU" + str(x)) for x in self.cgroup_logical_cpus_indexes],
            ["", "%", "%"] + ["%" for x in self.cgroup_logical_cpus_indexes],
        )

        #
        # MAIN LOOP
        # Process JSON sample and fill the GoogleChartsTimeSeries() object
        #

        cpu_quota_perc = self.__cgroup_get_cpu_quota_percentage()
        n_invalid_samples = 0
        # max_cpu_throttling = 0
        for i, s in enumerate(self.jdata):
            if i == 0:
                continue  # skip first sample

            try:
                ts = s["timestamp"]["UTC"]

                throttling = CMonitorGraphGenerator.cgroup_get_cpu_throttling(s)
                # max_cpu_throttling = max(max_cpu_throttling, throttling)
                all_cpus_row = [ts, cpu_quota_perc, throttling]
                for c in self.cgroup_logical_cpus_indexes:
                    # get data:
                    cpu_stats = s["cgroup_cpuacct_stats"]["cpu" + str(c)]
                    if "sys" in cpu_stats:
                        cpu_sys = cpu_stats["sys"]
                    else:
                        cpu_sys = 0
                    cpu_total = cpu_stats["user"] + cpu_sys

                    # append data:
                    cpu_stats_table[c].addRow([ts, cpu_stats["user"], cpu_sys])
                    all_cpus_row.append(cpu_total)

                all_cpus_table.addRow(all_cpus_row)
            except KeyError:  # avoid crashing if a key is not present in the dictionary...
                # print("Missing cgroup data while parsing sample %d" % i)
                n_invalid_samples += 1
                pass

        self.__print_data_loading_stats("cgroup CPU", n_invalid_samples)

        # Produce 1 graph for each CPU:
        for c in self.cgroup_logical_cpus_indexes:
            self.output_page.appendGoogleChart(
                GoogleChartsGraph(
                    data=cpu_stats_table[c],  # Data
                    graph_title="Logical CPU " + str(c) + " (from CGroup stats)",
                    combobox_label="cgroup_cpus",
                    combobox_entry="CPU" + str(c),
                    y_axes_titles=["CPU (%)"],
                    graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_TOTALS,
                    stack_state=True,
                )
            )

        # Also produce the "all CPUs" graph that includes some very useful KPIs like
        # - CPU limit imposed on Linux CFS scheduler
        # - Amount of CPU throttling
        # NOTE: when cgroups v2 are used, there's no per-CPU stat just the total CPU usage,
        #       so we change the title of the tab to reflect that
        graph_title = "CPU usage by index of CPU available inside cgroup" if self.cgroup_ver == 1 else "CPU usage measured in the cgroup"
        graph_title = f"{graph_title} ({self.__cgroup_get_cpu_quota_human_friendly()})"

        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=all_cpus_table,  # Data
                graph_title=graph_title,
                button_label="All CPUs" if self.cgroup_ver == 1 else "CPU",
                y_axes_titles=["CPU (%)", "CPU Throttling (%)"],
                graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_TOTALS,
                stack_state=False,
                # give evidence to CPU throttling by moving it on 2nd y axis:
                columns_for_2nd_yaxis=["Throttling"],
                y_axes_max_value=[None, 0],
            )
        )

        return

    def generate_baremetal_memory(self):
        # if baremetal memory data was not collected, just return:
        if "proc_meminfo" not in self.sample_template:
            return

        #
        # MAIN LOOP
        # Process JSON sample and build Google Chart-compatible Javascript variable
        # See https://developers.google.com/chart/interactive/docs/reference
        #

        mem_total_bytes = self.sample_template["proc_meminfo"]["MemTotal"]
        baremetal_memory_stats = GoogleChartsTimeSeries(["Timestamp", "Used", "Cached (DiskRead)", "Free"], ["", "B", "B", "B"])

        for i, s in enumerate(self.jdata):
            if i == 0:
                continue  # skip first sample
            meminfo_stats = s["proc_meminfo"]

            if meminfo_stats["MemTotal"] != mem_total_bytes:
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

            mf = meminfo_stats["MemFree"]
            mc = meminfo_stats["Cached"]

            baremetal_memory_stats.addRow(
                [
                    s["timestamp"]["UTC"],
                    int(mem_total_bytes - mf - mc),  # compute used memory
                    int(mc),  # cached
                    int(mf),  # free
                ]
            )

        # Produce the javascript:
        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=baremetal_memory_stats,  # Data
                graph_title="Memory usage in Bytes (from baremetal stats)",
                button_label="Memory",
                y_axes_titles=["Memory (B)"],
                graph_source=GRAPH_SOURCE_DATA_IS_BAREMETAL,
                stack_state=True,
            )
        )
        return

    def generate_cgroup_memory(self):
        # if cgroup data was not collected, just return:
        if "cgroup_memory_stats" not in self.sample_template:
            return

        #
        # MAIN LOOP
        # Process JSON sample and build Google Chart-compatible Javascript variable
        # See https://developers.google.com/chart/interactive/docs/reference
        #

        mem_limit_bytes = self.__cgroup_get_memory_limit()
        if mem_limit_bytes > 0:
            # it is possible to compute the "free" memory inside this cgroup and it's possible to have allocation failures
            # so in such case we add 2 more data series to the chart:
            cgroup_memory_stats = GoogleChartsTimeSeries(
                ["Timestamp", "Used", "Cached (DiskRead)", "Free", "Alloc Failures"], ["", "B", "B", "B", ""]
            )
            y_axes_max_value = [None, 0]
            columns_for_2nd_yaxis = ["Alloc Failures"]
            y_axes_titles = ["Memory (B)", "Alloc Failures"]
        else:
            # no memory limit... creating the "free" series (considering all the system memory as actual limit) likely produces weird
            # results out-of-scale so we do not place any "idle" column. The "alloc failures" column does not apply either.
            assert mem_limit_bytes == -1
            cgroup_memory_stats = GoogleChartsTimeSeries(["Timestamp", "Used", "Cached (DiskRead)"], ["", "B", "B"])
            y_axes_max_value = [None]  # use default GoogleChart logic
            columns_for_2nd_yaxis = None
            y_axes_titles = ["Memory (B)"]

        n_invalid_samples = 0
        # max_mfail = 0
        for i, s in enumerate(self.jdata):
            if i == 0:
                continue  # skip first sample

            try:
                # mu = memory actually Used
                # mc = memory used as Cache
                # mfail = memory alloc failures inside cgroup
                if self.cgroup_ver == 1:
                    mu = s["cgroup_memory_stats"]["stat.rss"]
                    mc = s["cgroup_memory_stats"]["stat.cache"]
                    mfail = s["cgroup_memory_stats"]["events.failcnt"]
                else:
                    # cgroups v2
                    mu = s["cgroup_memory_stats"]["stat.anon"]
                    mc = s["cgroup_memory_stats"]["stat.file"]
                    mfail = s["cgroup_memory_stats"]["events.oom_kill"]

                mfree = mem_limit_bytes - mu - mc
                # max_mfail = max(max_mfail, mfail)

                if mem_limit_bytes > 0:
                    cgroup_memory_stats.addRow(
                        [
                            s["timestamp"]["UTC"],
                            int(mu),
                            int(mc),
                            int(mfree),
                            mfail,
                        ]
                    )
                else:
                    cgroup_memory_stats.addRow(
                        [
                            s["timestamp"]["UTC"],
                            int(mu),
                            int(mc),
                        ]
                    )

            except KeyError as e:  # avoid crashing if a key is not present in the dictionary...
                print(f"Missing cgroup data while parsing {i}-th sample: {e}")
                n_invalid_samples += 1
                pass

        self.__print_data_loading_stats("cgroup memory", n_invalid_samples)

        # Produce the javascript:
        # NOTE: on 2nd axis we try to keep the plotted line below the ones that belong to the first axis (to avoid cluttering)
        #       and we also add some offset to deal with the case where "max_mfail is zero"
        # if mem_limit_bytes > 0:
        #    y_axes_max_value = [None, max_mfail * 5 + 10]
        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=cgroup_memory_stats,  # Data
                graph_title=f"Used memory in Bytes measured inside cgroup ({self.__cgroup_get_memory_limit_human_friendly()})",
                button_label="Memory",
                graph_source=GRAPH_SOURCE_DATA_IS_CGROUP_TOTALS,
                stack_state=True,
                y_axes_titles=y_axes_titles,
                columns_for_2nd_yaxis=columns_for_2nd_yaxis,
                y_axes_max_value=y_axes_max_value,
            )
        )

        return

    def generate_baremetal_avg_load(self):
        #
        # MAIN LOOP
        # Process JSON sample and build Google Chart-compatible Javascript variable
        # See https://developers.google.com/chart/interactive/docs/reference
        #

        num_baremetal_cpus = len(self.baremetal_logical_cpus_indexes)
        if num_baremetal_cpus == 0:
            num_baremetal_cpus = 1
        load_avg_stats = GoogleChartsTimeSeries(["Timestamp", "LoadAvg (1min)", "LoadAvg (5min)", "LoadAvg (15min)"])
        for i, s in enumerate(self.jdata):
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

            load_avg_stats.addRow(
                [
                    s["timestamp"]["UTC"],
                    100 * float(s["proc_loadavg"]["load_avg_1min"]) / num_baremetal_cpus,
                    100 * float(s["proc_loadavg"]["load_avg_5min"]) / num_baremetal_cpus,
                    100 * float(s["proc_loadavg"]["load_avg_15min"]) / num_baremetal_cpus,
                ]
            )

        # Produce the javascript:
        self.output_page.appendGoogleChart(
            GoogleChartsGraph(
                data=load_avg_stats,  # Data
                graph_title="Average Load (from baremetal stats)",
                button_label="Average Load",
                y_axes_titles=["Load (%)"],
                graph_source=GRAPH_SOURCE_DATA_IS_BAREMETAL,
                stack_state=False,
            )
        )
        return

    def generate_monitoring_summary(self):
        monitoring_summary = [
            # ( "User:", self.jheader["cmonitor"]["username"] ),   # not really useful
            ("Collected:", self.jheader["cmonitor"]["collecting"].replace(",", ", ")),
            # ( "Started sampling at:", self.sample_template["timestamp"]["datetime"] + " (Local)" ),   # not really useful
            ("Started sampling at:", self.jdata[0]["timestamp"]["UTC"] + " (UTC)"),
            ("Samples:", str(len(self.jdata))),
            ("Sampling Interval (s):", str(self.jheader["cmonitor"]["sample_interval_seconds"])),
            (
                "Total time sampled (hh:mm:ss):",
                str(datetime.timedelta(seconds=self.jheader["cmonitor"]["sample_interval_seconds"] * len(self.jdata))),
            ),
            ("Version (cmonitor_collector):", self.jheader["cmonitor"]["version"]),
            ("Version (cmonitor_chart):", VERSION_STRING),
        ]
        self.output_page.appendHtmlTable("Monitoring Summary", monitoring_summary)

    def __generate_monitored_summary_with_cpus(self, logical_cpus_indexes):
        # NOTE: unfortunately some useful information like:
        #        - RAM memory model/speed
        #        - Disk model/speed
        #        - NIC model/speed
        #       will not be available from inside a container, which is where cmonitor_collector usually runs...
        #       so we mostly show CPU stats:
        all_disks = []
        if "disks" in self.sample_template:
            all_disks = self.sample_template["disks"].keys()
        all_netdevices = []
        if "network_interfaces" in self.sample_template:
            all_netdevices = self.sample_template["network_interfaces"].keys()
        all_numanodes = []
        if "numa_nodes" in jheader:
            all_numanodes = list(jheader["numa_nodes"].keys())
            all_numanodes = [v.replace("node", "") for v in all_numanodes]

        cpu_model = "Unknown"
        bogomips = "Unknown"
        if "cpuinfo" in self.jheader:
            first_cpu = list(self.jheader["cpuinfo"].keys())[0]
            cpu_model = self.jheader["cpuinfo"][first_cpu]["model_name"]
            bogomips = str(self.jheader["cpuinfo"][first_cpu]["bogomips"])

        monitored_summary = [
            ("Hostname:", self.jheader["identity"]["hostname"]),
            ("OS:", self.jheader["os_release"]["pretty_name"]),
            ("CPU:", cpu_model),
            ("BogoMIPS:", bogomips),
            ("Monitored CPUs:", str(len(logical_cpus_indexes))),
            ("Monitored Disks:", str(len(all_disks))),
            ("Monitored Network Devices:", str(len(all_netdevices))),
            ("Monitored NUMA Nodes:", ",".join(all_numanodes)),
        ]
        return monitored_summary

    def generate_monitored_summary(self):
        if len(self.baremetal_logical_cpus_indexes) > 0:
            self.output_page.appendHtmlTable(
                "Monitored System Summary",
                self.__generate_monitored_summary_with_cpus(self.baremetal_logical_cpus_indexes),
            )
        elif len(self.cgroup_logical_cpus_indexes) > 0:
            self.output_page.appendHtmlTable(
                "Monitored System Summary",
                self.__generate_monitored_summary_with_cpus(self.cgroup_logical_cpus_indexes),
            )

    def generate_about_this(self):
        about_this = [
            ("Zoom:", "use left-click and drag"),
            ("Reset view:", "use right-click"),
            ("Generated by", '<a href="https://github.com/f18m/cmonitor">cmonitor</a>'),
        ]
        self.output_page.appendHtmlTable("About this", about_this, div_class="bottom_about_div")

    def generate_html(self, top_scorer, thread_filter):
        # baremetal stats:
        self.generate_baremetal_cpus()
        self.generate_baremetal_memory()
        self.generate_baremetal_network_traffic()
        self.generate_baremetal_disks_io()
        self.generate_baremetal_avg_load()

        # cgroup stats:
        self.generate_cgroup_cpus()
        self.generate_cgroup_memory()
        self.generate_cgroup_topN_procs(top_scorer, thread_filter)
        self.generate_cgroup_network_traffic()

        # HTML HEAD -- generate all the JS code to draw all the graphs created so far
        self.output_page.writeHtmlHead()

        # HTML BODY -- now we start actual HTML body which are just a few tables with buttons
        #              that invoke the JS code produced earlier inside the <HEAD>
        self.__make_jheader_nicer()
        self.output_page.startHtmlBody(self.cgroup_name, self.monitored_system, self.jheader, self.collected_threads)

        self.generate_monitoring_summary()
        self.generate_monitored_summary()
        self.generate_about_this()

        self.output_page.endHtmlBody()


# =======================================================================================================
# CLI options
# =======================================================================================================


def usage():
    """Provides commandline usage"""
    print("cmonitor_chart version {}".format(VERSION_STRING))
    print("Utility to post-process data recorded by 'cmonitor_collector' and")
    print("create a self-contained HTML file for visualizing that data.")
    print("Typical usage:")
    print("  %s --input=output_from_cmonitor_collector.json [--output=myreport.html]" % sys.argv[0])
    print("Required parameters:")
    print("  -i, --input=<file.json>    The JSON file to analyze.")
    print("Main options:")
    print("  -h, --help                 (this help)")
    print("  -v, --verbose              Be verbose.")
    print("  -o, --output=<file.html>   The name of the output HTML file. Defaults to the name of the JSON with .html extension.")
    print("  -u, --utc                  Plot data using UTC timestamps instead of local timezone.")
    print("  -t, --top-scorer=N         Plot the N most-CPU-hungry processes/threads. Default is 20. Zero means plot all.")
    print("  -f, --filter=<tname>       Plot only the threads whose name contains the <tname> string. By default no filtering is done.")
    print("      --version              Print version and exit.")
    sys.exit(0)


def parse_command_line():
    """Parses the command line and returns the configuration as dictionary object."""
    try:
        opts, remaining_args = getopt.getopt(
            sys.argv[1:],
            "hvvut",
            [
                "help",
                "verbose",
                "version",
                "output=",
                "input=",
                "utc",
                "top-scorer=",
                "filter=",
            ],
        )
    except getopt.GetoptError as err:
        # print help information and exit:
        print(str(err))  # will print something like "option -a not recognized"
        usage()  # will exit program

    global verbose
    global g_datetime
    input_json = ""
    output_html = ""
    top_scorer = 20
    thread_filter = ""
    for o, a in opts:
        if o in ("-i", "--input"):
            input_json = a
        elif o in ("-o", "--output"):
            output_html = a
        elif o in ("-h", "--help"):
            usage()
        elif o in ("-v", "--verbose"):
            verbose = True
        elif o in ("-u", "--utc"):
            # instead of default 'datetime' which means local timezone
            g_datetime = "UTC"
        elif o in ("-t", "--top-scorer"):
            try:
                top_scorer = int(a)
            except:
                print("Invalid argument for --top-scorer. Provide a number.")
                sys.exit(1)
        elif o in ("-f", "--filter"):
            thread_filter = a
        elif o in ("--version"):
            print("{}".format(VERSION_STRING))
            sys.exit(0)
        else:
            assert False, "unhandled option " + o + a

    if input_json == "":
        print("Please provide --input option (it is a required option)")
        sys.exit(os.EX_USAGE)

    # default value for output file
    if output_html == "":
        if input_json[-8:] == ".json.gz":
            output_html = input_json[:-8] + ".html"
        elif input_json[-5:] == ".json":
            output_html = input_json[:-5] + ".html"
        else:
            output_html = input_json + ".html"

    abs_input_json = input_json
    if not os.path.isabs(input_json):
        abs_input_json = os.path.join(os.getcwd(), input_json)

    return {
        "input_json": input_json,
        "output_html": output_html,
        "top_scorer": top_scorer,
        "thread_filter": thread_filter,
    }


# =======================================================================================================
# MAIN
# =======================================================================================================

if __name__ == "__main__":
    config = parse_command_line()
    start_time = time.time()

    # load the JSON
    entry = CmonitorCollectorJsonLoader().load(config["input_json"], this_tool_version=VERSION_STRING, min_num_samples=2)
    jheader = entry["header"]
    jdata = entry["samples"]
    if verbose:
        print("Found %d data samples" % len(jdata))

    print("Opening output file %s" % config["output_html"])
    graph_generator = CMonitorGraphGenerator(config["output_html"], jheader, jdata)
    graph_generator.generate_html(config["top_scorer"], config["thread_filter"])

    end_time = time.time()
    print("Completed processing of input JSON file of %d samples in %.3fsec. HTML output file is ready." % (len(jdata), end_time - start_time))
