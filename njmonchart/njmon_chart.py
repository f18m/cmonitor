#!/usr/bin/python3 

#
# njmonchart_linux.py
# Written as a modification of "njmonchart_aix_v7.py" from Nigel project: http://nmon.sourceforge.net/
#

import sys

# =======================================================================================================
# GLOBALs
# =======================================================================================================

buttonlist = []
chartnum = 1

# =======================================================================================================
# nchart_* routines to actually produce HTML+JavaScript
# =======================================================================================================


def nchart_start_javascript(file, title):
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
    file.write('     #button_table_col {border: darkgrey; border-style: solid; border-width: 3px; padding: 6px; margin: 6px;}\n')
    file.write('     #bottom_table {float:left; border: darkgrey; border-style: solid; border-width: 3px; padding: 6px; margin: 6px;}\n')
    file.write('  </style>\n')
    file.write('  <script type="text/javascript" src="https://www.google.com/jsapi"></script>\n')
    file.write('  <script type="text/javascript">\n')
    file.write('google.load("visualization", "1.1", {packages:["corechart"]});\n')
    file.write('google.setOnLoadCallback(setupCharts);\n')
    file.write('\n')
    file.write('function setupCharts() {\n')
    file.write('  var chart = null;\n')

    
def nchart_bubble_top(file, columnnames):
    ''' Before the graph data with datetime + multiple columns of data '''
    global chartnum 
    file.write('  var data_' + str(chartnum) + ' = google.visualization.arrayToDataTable([\n')
    file.write("[" + columnnames + "]\n")

    
def nchart_line_top(file, columnnames):
    ''' Before the graph data with datetime + multiple columns of data '''
    global chartnum 
    file.write('  var data_' + str(chartnum) + ' = google.visualization.arrayToDataTable([\n')
    file.write("[{type: 'datetime', label: 'Datetime'}," + columnnames + "],\n")

    
def nchart_bubble_bot(file, graphtitle):
    ''' After the JavaScript bubble graph data is output, the data is terminated and the bubble graph options set'''
    global chartnum
    file.write('  ]);\n')
    file.write('  var options_' + str(chartnum) + ' = {\n')
    file.write('    chartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n')
    file.write('    title: "' + graphtitle + '",\n')
    file.write('    hAxis: { title:"CPU seconds in Total"},\n')
    file.write('    vAxis: { title:"Character I/O in Total"},\n')
    file.write('    sizeAxis: {maxSize: 200},\n')
    file.write('    bubble: {textStyle: {fontSize: 15}}\n')
    file.write('  };\n')
    file.write('  document.getElementById("draw_' + str(chartnum) + '").addEventListener("click", function() {\n')
    file.write('  if (chart && chart.clearChart)\n')
    file.write('    chart.clearChart();\n')
    file.write('  chart = new google.visualization.BubbleChart(document.getElementById("chart_master"));\n')
    file.write('  chart.draw(data_' + str(chartnum) + ', options_' + str(chartnum) + ');\n')
    file.write('  });\n')
    chartnum += 1

    
def nchart_line_bot(file, graphtitle):
    ''' After the JavaSctipt line graph data is output, the data is terminated and the graph options set'''
    global next_graph_need_stacking
    global chartnum 
    file.write('  ]);\n')
    file.write('  var options_' + str(chartnum) + ' = {\n')
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
    file.write('  document.getElementById("draw_' + str(chartnum) + '").addEventListener("click", function() {\n')
    file.write('    if (chart && chart.clearChart)\n')
    file.write('      chart.clearChart();\n')
    file.write('    chart = new google.visualization.AreaChart(document.getElementById("chart_master"));\n')
    file.write('    chart.draw( data_' + str(chartnum) + ', options_' + str(chartnum) + ');\n')
    file.write('  });\n')
    chartnum += 1

    
def nchart_end_javascript(file, config):
    ''' Generic version using named arguments for 1 to 10 buttons for Server graphs - Finish off the web page '''
    file.write('}\n')
    file.write(config)
    file.write('  </script>\n')
    file.write('</head>\n')

    
def nchart_start_body(file, name, button_labels):
    file.write('<body bgcolor="#EEEEFF">\n')
    file.write('  <h1>Monitoring data for hostname: ' + name + '</h1>\n')
    file.write('  <div id="button_table">\n')
    file.write('  <table>\n')
    file.write('  <tr><td id="button_table_col">\n')
    file.write('    <button onclick="config()"><b>Configuration</b></button><br/>\n')
    
    # - - - loop through the buttons and change colours
    prevcol = 'black'
    for num, name in enumerate(button_labels, start=1):
        if name.startswith('Baremetal'):
            colour = 'red'
        elif name.startswith('Cgroup'):
            colour = 'orange'
        elif name == 'RAM Use':
            colour = 'blue'
        elif name == 'TotalDisk-MB':
            colour = 'brown'
        elif name == 'TotalNet-Bytes':
            colour = 'purple'
        elif name == 'IPC':
            colour = 'green'
        else:
            colour = 'black'
        if colour != prevcol:
            file.write('      </td><td id="button_table_col">\n')
        file.write('    <button id="draw_' + str(num) + '" style="color:' + colour + '"><b>' + name + '</b></button>\n')
        prevcol = colour
    file.write('  </td></tr>\n')
    file.write('  </table>\n')
    file.write('  </div>\n')
    file.write('  <p></p>\n')
    file.write('  <div id="chart_master"></div>\n')

    
def nchart_append_table(file, name, summary):
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


def nchart_end_body(file):
    file.write('</body>\n')
    file.write('</html>\n')

# =======================================================================================================
# Google Visualization charting routines
# =======================================================================================================


def googledate(date):
    ''' convert ISO date like 2017-08-21T20:12:30 to google date+time 2017,04,21,20,12,30 '''
    d = date[0:4] + "," + str(int(date[5:7]) - 1) + "," + date[8:10] + "," + date[11:13] + "," + date[14:16] + "," + date[17:19]
    return d


def graphit(web, column_names, data, title, button_label, stack_state):
    global next_graph_need_stacking
    nchart_line_top(web, column_names)
    web.write(data)  # write all data points of the graph
    next_graph_need_stacking = stack_state
    nchart_line_bot(web, title + (', STACKED GRAPH' if stack_state else ''))
    buttonlist.append(button_label)


def bubbleit(web, column_names, data, title, button_label):
    nchart_bubble_top(web, column_names)
    web.write(data)    
    nchart_bubble_bot(web, title)
    buttonlist.append(button_label)

# =======================================================================================================
# generate_* routines
# =======================================================================================================


def generate_config_javascript(jdata_first_sample):

    # ----- add config box 
    def configdump(section):
        newstr = '<h3>' + section + '</h3>\\\n'
        thing = jdata_first_sample[section]
        for label in thing:
            newstr = newstr + "%20s = %s<br>\\\n" % (label, str(thing[label]))
        return newstr
    
    config_str = ""
    config_str += '\nfunction config() {\n'
    config_str += '    var myWindow = window.open("", "MsgWindow", "width=1024, height=800");\n'
    config_str += '    myWindow.document.write("\\\n'
    config_str += '      <h2>Configuration of server and njmon data collection</h2>\\\n'
    config_str += configdump("os_release")
    config_str += configdump("identity")
    config_str += configdump("cgroup_config")
    config_str += configdump("njmon")
    config_str += configdump("timestamp")
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
    global buttonlist
    # - - - Disks
    dstr = ""
    for device in jdata[0]["disks"].keys():
        dstr = dstr + "'" + device + "',"
    dstr = dstr[:-1]
    nchart_line_top(web, dstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["disks"].keys():
            web.write(",%.1f" % (s["disks"][device]["time"]))
        web.write("]\n")
    nchart_line_bot(web, 'Disk Time')
    buttonlist.append("Disk-Time")
    
    dstr = ""
    for device in jdata[0]["disks"].keys():
        dstr = dstr + "'" + device + "+read','" + device + "-write',"
    dstr = dstr[:-1]
    nchart_line_top(web, dstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["disks"].keys():
            web.write(",%.1f,%.1f" % (
                     s["disks"][device]["rkb"],
                    -s["disks"][device]["wkb"]))
        web.write("]\n")
    nchart_line_bot(web, 'Disks MB/s')
    buttonlist.append("Disk-MB")
    
    dstr = ""
    for device in jdata[0]["disks"].keys():
        dstr = dstr + "'" + device + "+read','" + device + "-write',"
    dstr = dstr[:-1]
    nchart_line_top(web, dstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["disks"].keys():
            web.write(",%.1f,%.1f " % (
                     s["disks"][device]["reads"],
                    -s["disks"][device]["writes"]))
        web.write("]\n")
    nchart_line_bot(web, 'Disk blocks/s')
    buttonlist.append("Disk-blocks")
    return web


def generate_filesystems(web, jdata):
    global buttonlist
    fsstr = ""
    for fs in jdata[0]["filesystems"].keys():
        fsstr = fsstr + "'" + fs + "',"
    fsstr = fsstr[:-1]
    nchart_line_top(web, fsstr)
    for i, s in enumerate(jdata):
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for fs in s["filesystems"].keys():
            web.write(", %.1f" % (s["filesystems"][fs]["fs_full_percent"]))
        web.write("]\n")
    nchart_line_bot(web, 'File Systems Used percent')
    buttonlist.append("File Systems")
    return web


def generate_network_traffic(web, jdata):
    global buttonlist
    netstr = ""
    for device in jdata[0]["network_interfaces"].keys():
        netstr = netstr + "'" + device + "+in','" + str(device) + "-out',"
    netstr = netstr[:-1]
    nchart_line_top(web, netstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["network_interfaces"].keys():
            web.write(",%.1f,%.1f" % (
                 s["network_interfaces"][device]["ibytes"],
                -s["network_interfaces"][device]["obytes"]))
        web.write("]\n")
    nchart_line_bot(web, 'Network MB/s')
    buttonlist.append("Net-MB")
    
    netstr = ""
    for device in jdata[0]["network_interfaces"].keys():
        netstr = netstr + "'" + device + "+in','" + str(device) + "-out',"
    netstr = netstr[:-1]
    nchart_line_top(web, netstr)
    for i, s in enumerate(jdata):
        if i == 0:
            continue
        web.write(",['Date(%s)' " % (googledate(s['timestamp']['datetime'])))
        for device in s["network_interfaces"].keys():
            web.write(",%.1f,%.1f " % (
                 s["network_interfaces"][device]["ipackets"],
                -s["network_interfaces"][device]["opackets"]))
        web.write("]\n")
    nchart_line_bot(web, 'Network packets/s')
    buttonlist.append("Net-packets")
    return web


def generate_baremetal_cpus(web, jdata, num_logical_cpus, hostname):

    # prepare empty array
    baremetal_cpu_stats = []
    for c in range(num_logical_cpus):
        baremetal_cpu_stats.append("")
        
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        for c in range(num_logical_cpus):
            cpu_stats = s['stat']['cpu' + str(c)]
            baremetal_cpu_stats[c] += "['Date(%s)', %d, %d, %d, %d, %d, %d, %d, %d],\n" % (
                    googledate(s['timestamp']['datetime']),
                    cpu_stats['user'],
                    cpu_stats['nice'],
                    cpu_stats['sys'],
                    cpu_stats['idle'],
                    cpu_stats['iowait'],
                    cpu_stats['hardirq'],
                    cpu_stats['softirq'],
                    cpu_stats['steal']
                )
    
    # Produce the javascript:
    details = ' for hostname=' + hostname
    for c in range(num_logical_cpus):
        graphit(web, "'User','Nice','System','Idle','I/O wait','Hard IRQ','Soft IRQ','Steal'",
                baremetal_cpu_stats[c],  # Data
                'Logical CPU ' + str(c) + details + " (from baremetal stats)",  # Graph Title
                "Baremetal CPU" + str(c),  # Button Label
                stack_state=True)


def generate_cgroup_cpus(web, jdata, num_logical_cpus, hostname):
    if len(jdata) < 1:
        return
    if 'cgroup_cpuacct_stats' not in jdata[1]:
        return  # cgroup mode not enabled at collection time!
        
    # prepare empty array
    cgroup_cpu_stats = []
    for c in range(num_logical_cpus):
        cgroup_cpu_stats.append("")
        
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        for c in range(num_logical_cpus):
            cpu_stats = s['cgroup_cpuacct_stats']['cpu' + str(c)]
            if 'sys' in cpu_stats:
                cpu_sys = cpu_stats['sys']
            else:
                cpu_sys = 0
            cgroup_cpu_stats[c] += "['Date(%s)', %d, %d],\n" % (
                    googledate(s['timestamp']['datetime']),
                    cpu_stats['user'],
                    cpu_sys
                )

    # Produce the javascript:
    details = ' for hostname=' + hostname
    for c in range(num_logical_cpus):
        graphit(web, "'User','System'",
                cgroup_cpu_stats[c],  # Data
                'Logical CPU ' + str(c) + details + " (from cgroup stats)",  # Graph Title
                "Cgroup CPU" + str(c),  # Button Label
                stack_state=True)

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
    baremetal_memory_stats = ''
    divider, unit = choose_byte_divider(mem_total)

    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        mf = meminfo_stat_to_bytes(s['proc_meminfo']['MemFree'])
        mc = meminfo_stat_to_bytes(s['proc_meminfo']['Cached'])
        
        baremetal_memory_stats += "['Date(%s)', %d, %d, %d],\n" % (
                googledate(s['timestamp']['datetime']),
                (mem_total-mf-mc)/divider,
                mc/divider,
                mf/divider,
            )

    # Produce the javascript:
    details = ' for hostname=' + hostname
    graphit(web, "'Used','Cached (DiskRead)','Free'",
            baremetal_memory_stats,  # Data
            'Memory in ' + unit + details + " (from baremetal stats)",  # Graph Title
            "Baremetal Memory",  # Button Label
            stack_state=True)


def generate_cgroup_memory(web, jdata, hostname):
    #
    # MAIN LOOP
    # Process JSON sample and build Google Chart-compatible Javascript variable
    # See https://developers.google.com/chart/interactive/docs/reference
    #
     
    mem_total = jdata[0]['cgroup_memory_stats']['total_cache'] + \
                jdata[0]['cgroup_memory_stats']['total_rss'] 
    cgroup_memory_stats = ''
    divider, unit = choose_byte_divider(mem_total)
    
    for i, s in enumerate(jdata):
        if i == 0:
            continue  # skip first sample
        
        mu = s['cgroup_memory_stats']['total_rss']
        mc = s['cgroup_memory_stats']['total_cache']
        mfail = 0  # FIXME FIXME TODO
        cgroup_memory_stats += "['Date(%s)', %d, %d, %d],\n" % (
                googledate(s['timestamp']['datetime']),
                mu/divider,
                mc/divider,
                mfail,
            )

    # Produce the javascript:
    details = ' for hostname=' + hostname
    graphit(web, "'Used','Cached (DiskRead)','Alloc Failures'",
            cgroup_memory_stats,  # Data
            'Memory in ' + unit + details + " (from cgroup stats)",  # Graph Title
            "CGroup Memory",  # Button Label
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
    global chartnum
    global buttonlist
    next_graph_need_stacking = 0
    
    # These are flags used as function arguments
    stacked = 1
    unstacked = 0
    
    # initialise some useful content
    hostname = jdata[0]['identity']['hostname'] 
    details = ' for hostname=' + hostname

    num_logical_cpus = 0
    while ('cpu' + str(num_logical_cpus)) in jdata[0]['stat']:
        num_logical_cpus += 1
        
#     # - - - Disks
#         rkb=0.0
#         wkb=0.0
#         for disk in s["disks"].keys():
#                 rkb  = rkb  + s["disks"][disk]["rkb"]
#                 wkb = wkb + s["disks"][disk]["wkb"]
#         dtrw_data += ",['Date(%s)', %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']), rkb, -wkb)
#     
#         xfers=0
#         for disk in s["disks"].keys():
#             xfers += s["disks"][disk]["xfers"]
#         dtt_data += ",['Date(%s)', %.1f]\n" %(googledate(s['timestamp']['datetime']), xfers)
#     
#     # - - - Networks
#         ibytes=0
#         obytes=0
#         for net in s["network_interfaces"].keys():
#             ibytes   = ibytes   + s["network_interfaces"][net]["ibytes"]
#             obytes   = obytes   + s["network_interfaces"][net]["obytes"]
#         nio_data += ",['Date(%s)', %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']), ibytes, -obytes)
#     
#         ipackets=0.0
#         opackets=0.0
#         for net in s["network_interfaces"].keys():
#             ipackets = ipackets + s["network_interfaces"][net]["ipackets"]
#             opackets = opackets + s["network_interfaces"][net]["opackets"]
#         np_data += ",['Date(%s)', %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']), ipackets,-opackets)
    
    # - - - Top 20 Processes
    # Check if there are process stats in this njmon .json file as they are optional
    # start_processes, end_processes, process_data_found = generate_top20_procs(jdata)
    
    # - - - Top 20 Disks
    # tdisk, td_header, td_data = generate_top20_disks(jdata)
    
    # ----- MAIN SCRIPT CREAT WEB FILE -
    print("Opening output file %s" % outfile)
    web = open(outfile, "w")  # Open the output file
    nchart_start_javascript(web, 'Monitoring data' + details)
    
    # ----- add graphs 
    generate_baremetal_cpus(web, jdata, num_logical_cpus, hostname)
    generate_cgroup_cpus(web, jdata, num_logical_cpus, hostname)
    generate_baremetal_memory(web, jdata, hostname)
    generate_cgroup_memory(web, jdata, hostname)
    
    
    # if process_data_found:
    #    bubbleit(web, topprocs_title, topprocs,  'Top Processes Summary' + details, "TopSum")
    #    graphit(web, top_header, top_data,  'Top Procs by CPU time' + details, "TopProcs",unstacked)
  
    # graphit(web, td_header, td_data,  'Top Disks (mbps)' + details, "TopDisks",unstacked)
#     graphit(web, "'1_min_LoadAvg', '5_min_LoadAvg','15_min_LoadAvg'", la_data,  'Load Average' + details, "LoadAvg",unstacked)
    
    # web.write(generate_disks(jdata))
    # generate_filesystems(web, jdata)
    # generate_network_traffic(web,jdata)
    
    # web.write(config_button_str)
    jdata_first_sample = jdata[0]
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
    
    nchart_end_javascript(web, generate_config_javascript(jdata_first_sample))
    nchart_start_body(web, hostname, buttonlist)
    nchart_append_table(web, "Monitoring Summary", monitoring_summary)
    nchart_append_table(web, "Monitored System Summary", monitored_summary)
    web.write('<p>NOTE: to zoom use left-click and drag; to reset view use right-click.</p>\n')
    nchart_end_body(web)

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
