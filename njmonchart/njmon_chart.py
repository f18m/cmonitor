#!/usr/bin/python3 

#
# njmon_chart.py
# Written as a modification of "njmonchart_aix_v7.py" from Nigel project: http://nmon.sourceforge.net/
#
# Author: Francesco Montorsi
# Created: April 2019
#

import sys

# =======================================================================================================
# CONSTANTS
# =======================================================================================================

GRAPH_TYPE_BAREMETAL = 1
GRAPH_TYPE_CGROUP = 2

# =======================================================================================================
# TYPES
# =======================================================================================================


class Graph:

    def __init__(self, name, type):
        self.name = name
        self.type = type  # one of GRAPH_TYPE_BAREMETAL or GRAPH_TYPE_CGROUP
        

class Table(object):

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
    
    def writeTo(self, file):
        for r in self.rows:
            # assume first column is always the timestamp:
            row_text = "['Date(%s)'," % r[0]
            row_text += ','.join(str(x) for x in r[1:])
            row_text += '],\n'
            file.write(row_text)

# =======================================================================================================
# GLOBALs
# =======================================================================================================


g_graphs = []  # global list of Graph class instances
g_num_generated_charts = 1

# =======================================================================================================
# nchart_* routines to actually produce HTML+JavaScript
# =======================================================================================================


def nchart_start_js(file, title):
    ''' Head of the HTML webpage'''
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
    file.write('     #bottom_div {float:left; border: darkgrey; border-style: solid; border-width: 3px; padding: 6px; margin: 6px;}\n')
    file.write('     #button_table { border-collapse: collapse; }\n')
    file.write('     #button_table_col {border: darkgrey; border-style: solid; border-width: 3px; padding: 6px; margin: 6px;}\n')
    file.write('  </style>\n')
    file.write('  <script type="text/javascript" src="https://www.google.com/jsapi"></script>\n')
    file.write('  <script type="text/javascript">\n')
    file.write('google.load("visualization", "1.1", {packages:["corechart"]});\n')
    file.write('google.setOnLoadCallback(setupCharts);\n')
    file.write('\n')
    file.write('function setupCharts() {\n')
    file.write('  var chart = null;\n')

    
def nchart_start_js_bubble_graph(file, columnnames):
    ''' Before the graph data with datetime + multiple columns of data '''
    global g_num_generated_charts 
    file.write('  var data_' + str(g_num_generated_charts) + ' = google.visualization.arrayToDataTable([\n')
    file.write("[" + columnnames + "]\n")

    
def nchart_start_js_line_graph(file, columnnames):
    ''' Before the graph data with datetime + multiple columns of data '''
    global g_num_generated_charts 
    file.write('  var data_' + str(g_num_generated_charts) + ' = google.visualization.arrayToDataTable([\n')
    
    assert(columnnames[0] == 'Timestamp')
    columns_text = "{type: 'datetime', label: 'Datetime'}"
    for col in columnnames[1:]:
        columns_text += ",'%s'" % col
    
    file.write("[" + columns_text + "],\n")


def nchart_write_js_graph_data(web, table_data):
    table_data.writeTo(web)  # write all data points of the graph

    
def nchart_end_js_bubble_graph(file, graphtitle):
    ''' After the JavaScript bubble graph data is output, the data is terminated and the bubble graph options set'''
    global g_num_generated_charts
    file.write('  ]);\n')
    file.write('  var options_' + str(g_num_generated_charts) + ' = {\n')
    file.write('    chartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n')
    file.write('    title: "' + graphtitle + '",\n')
    file.write('    hAxis: { title:"CPU seconds in Total"},\n')
    file.write('    vAxis: { title:"Character I/O in Total"},\n')
    file.write('    sizeAxis: {maxSize: 200},\n')
    file.write('    bubble: {textStyle: {fontSize: 15}}\n')
    file.write('  };\n')
    file.write('  document.getElementById("draw_' + str(g_num_generated_charts) + '").addEventListener("click", function() {\n')
    file.write('  if (chart && chart.clearChart)\n')
    file.write('    chart.clearChart();\n')
    file.write('  chart = new google.visualization.BubbleChart(document.getElementById("chart_master"));\n')
    file.write('  chart.draw(data_' + str(g_num_generated_charts) + ', options_' + str(g_num_generated_charts) + ');\n')
    file.write('  });\n')
    g_num_generated_charts += 1

    
def nchart_end_js_line_graph(file, graphtitle):
    ''' After the JavaSctipt line graph data is output, the data is terminated and the graph options set'''
    global next_graph_need_stacking
    global g_num_generated_charts 
    file.write('  ]);\n')
    file.write('  var options_' + str(g_num_generated_charts) + ' = {\n')
    file.write('    chartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n')
    file.write('    title: "' + graphtitle + '",\n')
    file.write('    focusTarget: "category",\n')
    file.write('    hAxis: { gridlines: { color: "lightgrey", count: 30 } },\n')
    file.write('    vAxis: { gridlines: { color: "lightgrey", count: 11 } },\n')
    file.write('    explorer: { actions: ["dragToZoom", "rightClickToReset"],\n')
    file.write('    axis: "horizontal", keepInBounds: true, maxZoomIn: 20.0 },\n')
    if next_graph_need_stacking:
        file.write('    isStacked:  1\n')
        next_graph_need_stacking = 0
    else:
        file.write('    isStacked:  0\n')
    file.write('  };\n')
    file.write('  document.getElementById("draw_' + str(g_num_generated_charts) + '").addEventListener("click", function() {\n')
    file.write('    if (chart && chart.clearChart)\n')
    file.write('      chart.clearChart();\n')
    file.write('    chart = new google.visualization.AreaChart(document.getElementById("chart_master"));\n')
    file.write('    chart.draw( data_' + str(g_num_generated_charts) + ', options_' + str(g_num_generated_charts) + ');\n')
    file.write('  });\n')
    g_num_generated_charts += 1

    
def nchart_end_js(file, config):
    ''' Generic version using named arguments for 1 to 10 buttons for Server graphs - Finish off the web page '''
    file.write('}\n')
    file.write(config)
    file.write('  </script>\n')
    file.write('</head>\n')

    
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
    file.write('    <button onclick="config()"><b>Configuration</b></button><br/>\n')
    file.write('  </td><td id="button_table_col">\n')

    def write_buttons_for_graph_type(type):
        # find in all graphs registered so far all those related to the CGROUP
        for num, graph in enumerate(g_graphs, start=1):
            if graph.type == type:
                if graph.name.startswith('CPU') or graph.name.endswith('CPUs'):
                    colour = 'red'
                elif graph.name.startswith('Memory'):
                    colour = 'orange'
                else:
                    colour = 'black'
                file.write('    <button id="draw_' + str(num) + '" style="color:' + colour + '"><b>' + graph.name + '</b></button>\n')

    write_buttons_for_graph_type(GRAPH_TYPE_CGROUP)
    file.write('      </td><td id="button_table_col">\n')
    write_buttons_for_graph_type(GRAPH_TYPE_BAREMETAL)
        
    file.write('  </td></tr>\n')
    file.write('  </table>\n')
    file.write('  </div>\n')
    file.write('  <p></p>\n')
    file.write('  <div id="chart_master"></div>\n')

    
def nchart_append_html_table(file, name, summary):
    file.write('  <div id="bottom_table">\n')
    file.write('    <h3>' + name + '</h3>\n')
    file.write('    <table>\n')
    file.write('    <tr><td><ul>\n')
    for i, entry in enumerate(summary, start=1):
        file.write("      <li>" + entry + "</li>\n")
        if (i % 4) == 0:
            file.write("      </ul></td><td><ul>\n")
    file.write('    </ul></td></tr>\n')
    file.write('    </table>\n')
    file.write('  </div>\n')


def nchart_end_html_body(file):
    file.write('</body>\n')
    file.write('</html>\n')

# =======================================================================================================
# Google Visualization charting routines
# =======================================================================================================


def googledate(date):
    ''' convert ISO date like 2017-08-21T20:12:30 to google date+time 2017,04,21,20,12,30 '''
    d = date[0:4] + "," + str(int(date[5:7]) - 1) + "," + date[8:10] + "," + date[11:13] + "," + date[14:16] + "," + date[17:19]
    return d


def graphit(web, table_data, title, button_label, graph_type, stack_state):
    global next_graph_need_stacking
    global g_graphs
    
    # declare JS variables:
    nchart_start_js_line_graph(web, table_data.getListColumnNames())
    nchart_write_js_graph_data(web, table_data)
    next_graph_need_stacking = stack_state
    nchart_end_js_line_graph(web, title + (', STACKED graph' if stack_state else ''))
    
    # register this graph into globals
    g_graphs.append(Graph(button_label, graph_type))



def bubbleit(web, column_names, data, title, button_label, graph_type):
    global g_graphs
    
    # declare JS variables:
    nchart_start_js_bubble_graph(web, column_names)
    nchart_write_js_graph_data(web, data)
    nchart_end_js_bubble_graph(web, title)
    
    # register this graph into globals
    g_graphs.append(Graph(button_label, graph_type))

# =======================================================================================================
# generate_* routines
# =======================================================================================================


def generate_config_js(jdata_first_sample):

    # ----- add config box 
    def configdump(section):
        newstr = '<h3>' + section + '</h3>\\\n'
        thing = jdata_first_sample[section]
        for label in thing:
            newstr = newstr + "%20s = %s<br>\\\n" % (label, str(thing[label]))
        return newstr
    
    def sizeof_fmt(num, suffix='B'):
        for unit in ['', 'Ki', 'Mi', 'Gi', 'Ti', 'Pi', 'Ei', 'Zi']:
            if abs(num) < 1024.0:
                return "%3.1f%s%s" % (num, unit, suffix)
            num /= 1024.0
        return "%.1f%s%s" % (num, 'Yi', suffix)
    
    # provide some human-readable config files:
    avail_cpus = jdata_first_sample['cgroup_config']['cpus'].split(',')
    jdata_first_sample['cgroup_config']['cpus_num_allowed'] = len(avail_cpus)
    jdata_first_sample['cgroup_config']['memory_limit_bytes_human_readable'] = sizeof_fmt(int(jdata_first_sample['cgroup_config']['memory_limit_bytes']))
          
    config_str = ""
    config_str += '\nfunction config() {\n'
    config_str += '    var myWindow = window.open("", "MsgWindow", "width=1024, height=800, toolbar=no");\n'
    config_str += '    myWindow.document.write("\\\n'
    config_str += '    <html><head>\\\n'
    config_str += '      <title>Configuration</title>\\\n'
    config_str += '    </head><body>\\\n'
    config_str += '      <h2>Configuration of server and njmon data collection</h2>\\\n'
    config_str += configdump("os_release")
    config_str += configdump("identity")
    config_str += configdump("cgroup_config")
    config_str += configdump("njmon")
    config_str += configdump("timestamp")
    config_str += '    </body></html>\\\n'
    config_str += '");\n}\n\n'
    return config_str


def generate_top20_procs(jdata):
    # - - - Top 20 Processes
    # Check if there are process stats in this njmon .json file as they are optional
    start_processes = "unknown"
    end_processes = "unknown"
    try:
        start_processes = len(jdata[0]["processes"])
        end_processes = len(jdata[-1]["processes"])
        process_data_found = True
    except:
        process_data_found = False
    
    if process_data_found:
        top = {}  # start with empty dictionary
        for sam in jdata:
            for process in sam["processes"]:
                entry = sam['processes'][process]
                if entry['ucpu_time'] != 0.0 and entry['scpu_time'] != 0.0:
                    # print("%20s pid=%d ucpu=%0.3f scpu=%0.3f mem=%0.3f io=%0.3f"%(entry['name'],entry['pid'],
                    #        entry['ucpu_time'],entry['scpu_time'],
                    #        entry['real_mem_data']+entry['real_mem_text'],
                    #        entry['inBytes']+entry['outBytes']))
                    try:  # update the current entry
                        top[entry['name']]["cpu"] += entry['ucpu_time'] + entry['scpu_time']
                        top[entry['name']]["io"] += entry['inBytes'] + entry['inBytes']
                        top[entry['name']]["mem"] += entry['real_mem_data'] + entry['real_mem_text']
                    except:  # no current entry so add one
                        top.update({entry['name']: { "cpu": entry['ucpu_time'] + entry['scpu_time'],
                                  "io": entry['inBytes'] + entry['outBytes'],
                                  "mem": entry['real_mem_data'] + entry['real_mem_text']} })

        def sort_key(d):
            return top[d]['cpu']
    
        topprocs = ""
        tops = []
        for i, proc in enumerate(sorted(top, key=sort_key, reverse=True)):
            p = top[proc]
            # print("%20s cpu=%0.3f io=%0.3f mem=%0.3f"%(proc, p['cpu'],p['io'],p['mem']))
            topprocs += ",['%s',%.1f,%.1f,'%s',%.1f]\n" % (proc, p['cpu'], p['io'], proc, p['mem'])
            tops.append(proc)
            if i >= 20:  # Only graph the top 20
                break
    
        topprocs_title = "'Command', 'CPU seconds', 'CharIO', 'Type', 'Memory KB'"
    
        top_header = ""
        for proc in tops:
            top_header += "'" + proc + "',"
        top_header = top_header[:-1]
    
        top_data = ""
        for sam in jdata:
            top_data += ",['Date(%s)'" % (googledate(sam['timestamp']['datetime']))
            for item in tops:
                bytes = 0
                for proc in sam['processes']:
                    p = sam['processes'][proc]
                    if p['name'] == item:
                        bytes += p['ucpu_time'] + p['scpu_time']
                top_data += ", %.1f" % (bytes)
            top_data += "]\n"
        # print(top_header)
        # print(top_data)
    return (start_processes, end_processes, process_data_found)


def generate_top20_disks(jdata):
    tdisk = {}  # start with empty dictionary
    for sam in jdata:
            for disk in sam["disks"]:
                entry = sam['disks'][disk]
                bytes = entry['rkb'] + entry['wkb']
                if bytes != 0:
                    # print("disk=%s total bytes=%.1f"%(disk,bytes))
                    try:  # update the current entry
                        tdisk[entry[disk]] += bytes
                    except:  # no current entry so add one
                        tdisk.update({disk: bytes})

    def sort_dkey(d):
            return tdisk[d]
    
    topdisks = []
    for i, disk in enumerate(sorted(tdisk, key=sort_dkey, reverse=True)):
        d = tdisk[disk]
        # print("disk=%s total bytes=%.1f"%(disk,bytes))
        topdisks.append(disk)
        if i >= 20:  # Only graph the top 20
            break
    # print(topdisks)
    
    td_header = ""
    for disk in topdisks:
        td_header += "'" + disk + "',"
    td_header = td_header[:-1]
    
    td_data = ""
    for sam in jdata:
        td_data += ",['Date(%s)'" % (googledate(sam['timestamp']['datetime']))
        for item in topdisks:
            bytes = sam['disks'][item]['rkb'] + sam['disks'][item]['wkb']
            td_data += ", %.1f" % (bytes)
        td_data += "]\n"
    # print(td_header)
    # print(td_data)
    return (tdisk, td_header, td_data)


def generate_disks(web, jdata):
    global g_graphs
    # - - - Disks
    dstr = ""
    for device in jdata[0]["disks"].keys():
        dstr = dstr + "'" + device + "',"
    dstr = dstr[:-1]
    nchart_start_js_line_graph(web, dstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["disks"].keys():
            web.write(",%.1f" % (s["disks"][device]["time"]))
        web.write("]\n")
    nchart_end_js_line_graph(web, 'Disk Time')
    g_graphs.append("Disk-Time")
    
    dstr = ""
    for device in jdata[0]["disks"].keys():
        dstr = dstr + "'" + device + "+read','" + device + "-write',"
    dstr = dstr[:-1]
    nchart_start_js_line_graph(web, dstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["disks"].keys():
            web.write(",%.1f,%.1f" % (
                     s["disks"][device]["rkb"],
                    -s["disks"][device]["wkb"]))
        web.write("]\n")
    nchart_end_js_line_graph(web, 'Disks MB/s')
    g_graphs.append("Disk-MB")
    
    dstr = ""
    for device in jdata[0]["disks"].keys():
        dstr = dstr + "'" + device + "+read','" + device + "-write',"
    dstr = dstr[:-1]
    nchart_start_js_line_graph(web, dstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["disks"].keys():
            web.write(",%.1f,%.1f " % (
                     s["disks"][device]["reads"],
                    -s["disks"][device]["writes"]))
        web.write("]\n")
    nchart_end_js_line_graph(web, 'Disk blocks/s')
    g_graphs.append("Disk-blocks")
    return web


def generate_filesystems(web, jdata):
    global g_graphs
    fsstr = ""
    for fs in jdata[0]["filesystems"].keys():
        fsstr = fsstr + "'" + fs + "',"
    fsstr = fsstr[:-1]
    nchart_start_js_line_graph(web, fsstr)
    for i, s in enumerate(jdata):
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for fs in s["filesystems"].keys():
            web.write(", %.1f" % (s["filesystems"][fs]["fs_full_percent"]))
        web.write("]\n")
    nchart_end_js_line_graph(web, 'File Systems Used percent')
    g_graphs.append("File Systems")
    return web


def generate_network_traffic(web, jdata, hostname):
    global g_graphs
    
    netcols = ['Timestamp']
    for device in jdata[0]["network_interfaces"].keys():
        netcols.append(str(device) + "+in")
        netcols.append(str(device) + "-out")

    # convert from bytes to MB
    divider = 1000 * 1000

    #
    # MAIN LOOP
    # Process JSON sample and fill the Table() object
    #
    
    
    # MB/sec
    
    net_table = Table(netcols)
    for i, s in enumerate(jdata):
        if i == 0:
            continue

        row = [ googledate(s['timestamp']['datetime']) ]
        for device in s["network_interfaces"].keys():
            row.append(s["network_interfaces"][device]["ibytes"]/divider)
            row.append(-s["network_interfaces"][device]["obytes"]/divider)
        net_table.addRow(row)

    graphit(web,
            net_table,  # Data
            'Network Traffic in MB/s for ' + hostname + " (from baremetal stats)",  # Graph Title
            'Network Traffic (MB/s)',  # Button Label
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False)
    
    # PPS
    
    net_table = Table(netcols)
    for i, s in enumerate(jdata):
        if i == 0:
            continue

        row = [ googledate(s['timestamp']['datetime']) ]
        for device in s["network_interfaces"].keys():
            row.append(s["network_interfaces"][device]["ipackets"])
            row.append(-s["network_interfaces"][device]["opackets"])
        net_table.addRow(row)

    graphit(web,
            net_table,  # Data
            'Network Traffic in PPS for ' + hostname + " (from baremetal stats)",  # Graph Title
            'Network Traffic (PPS)',  # Button Label
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False)
    
    return web


def generate_baremetal_cpus(web, jdata, logical_cpus_indexes, hostname):

    # prepare empty tables
    baremetal_cpu_stats = {}
    for c in logical_cpus_indexes:
        baremetal_cpu_stats[c] = Table(['Timestamp', 'User', 'Nice', 'System', 'Idle', 'I/O wait', 'Hard IRQ', 'Soft IRQ', 'Steal'])
        
    all_cpus_table = Table(['Timestamp'] + [('CPU' + str(x)) for x in logical_cpus_indexes])
    
    #
    # MAIN LOOP
    # Process JSON sample and fill the Table() object
    #

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample

        ts = googledate(s['timestamp']['datetime'])
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
    details = ' for hostname=' + hostname
    for c in logical_cpus_indexes:
        graphit(web,
                baremetal_cpu_stats[c],  # Data
                'Logical CPU ' + str(c) + details + " (from baremetal stats)",  # Graph Title
                "CPU" + str(c),  # Button Label
                graph_type=GRAPH_TYPE_BAREMETAL,
                stack_state=True)

    # Also produce the "all CPUs" graph
    graphit(web,
            all_cpus_table,  # Data
            'Logical CPU ' + details + " (from cgroup stats)",  # Graph Title
            "All CPUs",  # Button Label
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False)
    

def generate_cgroup_cpus(web, jdata, logical_cpus_indexes, hostname):
    if len(jdata) < 1:
        return
    if 'cgroup_cpuacct_stats' not in jdata[1]:
        return  # cgroup mode not enabled at collection time!
        
    # prepare empty tables
    cpu_stats_table = {}
    for c in logical_cpus_indexes:
        cpu_stats_table[c] = Table(['Timestamp', 'User', 'System'])
        
    all_cpus_table = Table(['Timestamp'] + [('CPU' + str(x)) for x in logical_cpus_indexes])
        
    #
    # MAIN LOOP
    # Process JSON sample and fill the Table() object
    #

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        ts = googledate(s['timestamp']['datetime'])
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
    details = ' for hostname=' + hostname
    for c in logical_cpus_indexes:
        graphit(web,
                cpu_stats_table[c],  # Data
                'Logical CPU ' + str(c) + details + " (from cgroup stats)",  # Graph Title
                "CPU" + str(c),  # Button Label
                graph_type=GRAPH_TYPE_CGROUP,
                stack_state=True)

    # Also produce the "all CPUs" graph
    graphit(web,
            all_cpus_table,  # Data
            'Logical CPU ' + details + " (from cgroup stats)",  # Graph Title
            "All CPUs",  # Button Label
            graph_type=GRAPH_TYPE_CGROUP,
            stack_state=False)


def choose_byte_divider(mem_total):
    divider = 1
    unit = 'Bytes'
    if mem_total > 99E9:
        divider = 1E9
        unit = 'GB'
    elif mem_total > 99E6:
        divider = 1E6
        unit = 'MB'
    return (divider, unit)


def generate_baremetal_memory(web, jdata, hostname):
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
    
    def meminfo_stat_to_bytes(value):
        # NOTE: all values collected are in kB
        return value * 1E3

    mem_total = meminfo_stat_to_bytes(jdata[0]['proc_meminfo']['MemTotal'])
    baremetal_memory_stats = Table(['Timestamp', 'Used', 'Cached (DiskRead)', 'Free'])
    divider, unit = choose_byte_divider(mem_total)

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        mf = meminfo_stat_to_bytes(s['proc_meminfo']['MemFree'])
        mc = meminfo_stat_to_bytes(s['proc_meminfo']['Cached'])
        
        baremetal_memory_stats.addRow([
                googledate(s['timestamp']['datetime']),
                (mem_total - mf - mc) / divider,
                mc / divider,
                mf / divider,
            ])

    # Produce the javascript:
    graphit(web,
            baremetal_memory_stats,  # Data
            'Memory in ' + unit + ' for hostname=' + hostname + " (from baremetal stats)",  # Graph Title
            "Memory Usage",  # Button Label
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=True)


def generate_cgroup_memory(web, jdata, hostname):
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
     
    mem_total = jdata[0]['cgroup_memory_stats']['total_cache'] + \
                jdata[0]['cgroup_memory_stats']['total_rss'] 
    cgroup_memory_stats = Table(['Timestamp', 'Used', 'Cached (DiskRead)', 'Alloc Failures'])
    divider, unit = choose_byte_divider(mem_total)
    
    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        mu = s['cgroup_memory_stats']['total_rss']
        mc = s['cgroup_memory_stats']['total_cache']
        mfail = s['cgroup_memory_stats']['failcnt']
        cgroup_memory_stats.addRow([
                googledate(s['timestamp']['datetime']),
                mu / divider,
                mc / divider,
                mfail,
            ])

    # Produce the javascript:
    graphit(web,
            cgroup_memory_stats,  # Data
            'Memory in ' + unit + ' for hostname=' + hostname + " (from cgroup stats)",  # Graph Title
            "Memory Usage",  # Button Label
            graph_type=GRAPH_TYPE_CGROUP,
            stack_state=False)

def generate_load_avg(web, jdata, hostname):
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
     
    load_avg_stats = Table(['Timestamp', 'LoadAvg (1min)', 'LoadAvg (5min)', 'LoadAvg (15min)'])
    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        load_avg_stats.addRow([
                googledate(s['timestamp']['datetime']),
                s['proc_loadavg']['load_avg_1min'],
                s['proc_loadavg']['load_avg_5min'],
                s['proc_loadavg']['load_avg_15min']
            ])

    # Produce the javascript:
    graphit(web,
            load_avg_stats,  # Data
            'Average Load ' + ' for hostname=' + hostname + " (from baremetal stats)",  # Graph Title
            "Average Load",  # Button Label
            graph_type=GRAPH_TYPE_BAREMETAL,
            stack_state=False)



# =======================================================================================================
# MAIN SCRIPT PREPARE DATA
# =======================================================================================================


def main_process_file(cmd, infile, outfile):
    # read the raw .json as text
    print("Loading %s" % infile)
    f = open(infile, "r")
    text = f.read()
    f.close()
    # fix up the end of the file if it is not complete
    ending = text[-3:-1]  # The last two character
    if ending == "},":  # the data capture is still running or halted
        text = text[:-2] + "\n ]\n}\n"
    
    # Convert the text to json and extract the stats
    import json
    entry = []
    entry = json.loads(text)  # convert text to JSON
    jdata = entry["samples"]  # removes outer parts so we have a list of snapshot dictionaries
    
    # - - - - - Start nchart functions
    next_graph_need_stacking = 0
    
    # These are flags used as function arguments
    stacked = 1
    unstacked = 0
    
    # initialise some useful content
    hostname = jdata[0]['identity']['hostname'] 
    details = ' for hostname=' + hostname

    jdata_first_sample = jdata[0]
    logical_cpus_indexes = []
    for key in jdata_first_sample['stat']:
        if key.startswith('cpu'):
            cpuIdx = int(key[3:])
            # print("%s %s" %(key, cpuIdx))
            logical_cpus_indexes.append(cpuIdx) 
    print("Found %d CPUs in input file: %s" % (len(logical_cpus_indexes), ', '.join(str(x) for x in logical_cpus_indexes)))
    
    # ----- MAIN SCRIPT CREAT WEB FILE -
    print("Opening output file %s" % outfile)
    web = open(outfile, "w")  # Open the output file
    nchart_start_js(web, 'Monitoring data' + details)
    
    # JAVASCRIPT GRAPHS
    generate_baremetal_cpus(web, jdata, logical_cpus_indexes, hostname)
    generate_cgroup_cpus(web, jdata, logical_cpus_indexes, hostname)
    generate_baremetal_memory(web, jdata, hostname)
    generate_cgroup_memory(web, jdata, hostname)
    generate_network_traffic(web, jdata, hostname)
    generate_load_avg(web, jdata, hostname)
    
    # if process_data_found:
    #    bubbleit(web, topprocs_title, topprocs,  'Top Processes Summary' + details, "TopSum")
    #    graphit(web, top_header, top_data,  'Top Procs by CPU time' + details, "TopProcs",unstacked)
  
    # graphit(web, td_header, td_data,  'Top Disks (mbps)' + details, "TopDisks",unstacked)
    # web.write(generate_disks(jdata))
    # generate_filesystems(web, jdata)
    
    monitoring_summary = [
        "Monitoring launched as: " + jdata_first_sample["njmon"]["njmon_command"],
        '<a href="https://github.com/f18m/nmon-cgroup-aware">njmon-cgroup-aware</a>: ' + jdata_first_sample["njmon"]["njmon_version"],
        "DateTime (Local): " + jdata_first_sample["timestamp"]["datetime"],
        "DateTime (UTC): " + jdata_first_sample["timestamp"]["UTC"],
        "Snapshots: " + str(len(jdata)),
        "Snapshot Interval (s): " + str(jdata_first_sample["timestamp"]["snapshot_seconds"]),
        "User: " + jdata_first_sample["njmon"]["username"],
    ]
    monitored_summary = [
        "Hostname: " + jdata_first_sample["identity"]["hostname"],
        "CPU family: " + jdata_first_sample["lscpu"]["model_name"],
        "OS: " + jdata_first_sample["os_release"]["pretty_name"],
    ]
    
    nchart_end_js(web, generate_config_js(jdata_first_sample))
    
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
        outfile = infile[:-4] + "html"

    main_process_file(cmd, infile, outfile)
