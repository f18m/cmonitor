#!/usr/bin/python3 

#
# njmonchart_linux.py
# Written as a modification of "njmonchart_aix_v7.py" from Nigel project: http://nmon.sourceforge.net/
#

import sys
    

def nchart_start_script(file, title):
    ''' Head of the HTML webpage'''
    file.write('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">\n')
    file.write('<html xmlns="http://www.w3.org/1999/xhtml">' + '\n')
    file.write('<head>' + '\n')
    file.write('  <title>' + title + '</title>\n')
    file.write('  <style>\n')
    file.write('     html,body {height:85%;}\n')
    file.write('     h3 {margin: 0px;}\n')
    file.write('     ul {margin: 0 0 0 0;padding-left: 20px;}\n')
    file.write('     #chart_master {width:100%; height:85%;}\n')
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

    
def nchart_end_script(file, config):
    ''' Generic version using named arguments for 1 to 10 buttons for Server graphs - Finish off the web page '''
    file.write('}\n')
    file.write(config)
    file.write('  </script>\n')
    file.write('</head>\n')

    
def nchart_start_body(file, name, buttons):
    file.write('<body bgcolor="#EEEEFF">\n')
    file.write('  <h1>Monitoring data for hostname: ' + name + '</h1>\n')
    file.write('  <button onclick="config()"><b>Full Configuration</b></button>\n')
    # - - - loop through the buttons and change colours
    colour = 'black' 
    for num, name in enumerate(buttons, start=1):
       if(name == 'CPU Core'):
           colour = 'red'
       if(name == 'RAM Use'):
           colour = 'blue'
       if(name == 'SysCalls'):
           colour = 'orange'
       if(name == 'TotalDisk-MB'):
           colour = 'brown'
       if(name == 'TotalNet-Bytes'):
           colour = 'purple'
       if(name == 'IPC'):
           colour = 'green'
       file.write('  <button id="draw_' + str(num) + '" style="color:' + colour + '"><b>' + name + '</b></button>\n')
    file.write('  <p></p>\n')
    file.write('  <div id="chart_master"></div>\n')

    
def nchart_append_table(file, name, summary):
    file.write('  <div id="bottom_table">\n')
    file.write('    <h3>' + name + '</h3>\n')
    file.write('    <table>\n')
    file.write('    <tr><td><ul>\n')
    for i, entry in enumerate(summary, start=1):
        file.write("      <li>" + entry + "</li>\n")
        if((i % 4) == 0):
            file.write("      </ul></td><td><ul>\n")
    file.write('    </ul></td></tr>\n')
    file.write('    </table>\n')
    file.write('  </div>\n')


def nchart_end_body(file):
    file.write('</body>\n')
    file.write('</html>\n')


# convert ISO date like 2017-08-21T20:12:30 to google date+time 2017,04,21,20,12,30
def googledate(date):
    d = date[0:4] + "," + str(int(date[5:7]) - 1) + "," + date[8:10] + "," + date[11:13] + "," + date[14:16] + "," + date[17:19]
    return d
# - - - - - The nchart function End


def graphit(web, column_names, data, title, button, stack_state):
    global next_graph_need_stacking
    nchart_line_top(web, column_names)
    web.write(data)    # write all data points of the graph
    next_graph_need_stacking = stack_state
    nchart_line_bot(web, title)
    buttonlist.append(button)

 
def bubbleit(web, column_names, data, title, button):
    nchart_bubble_top(web, column_names)
    web.write(data)    
    nchart_bubble_bot(web, title)
    buttonlist.append(button)


def generate_config(jdata_first_sample):

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
    
    if (process_data_found):
        top = {}  # start with empty dictionary
        for sam in jdata:
            for process in sam["processes"]:
                entry = sam['processes'][process]
                if (entry['ucpu_time'] != 0.0 and entry['scpu_time'] != 0.0):
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
            if(i >= 20):  # Only graph the top 20
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
                    if(p['name'] == item):
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
                if (bytes != 0):
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
        if(i >= 20):  # Only graph the top 20
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

# ----- MAIN SCRIPT PREPARE DATA -


buttonlist = []
chartnum = 1


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
    cpu_data = ""
    mhz_data = ""
    pcpu_data = ""
    rq_data = ""
    sc_data = ""
    ps_data = ""
    rw_data = ""
    rwmb_data = ""
    fe_data = ""
    ds_data = ""
    la_data = ""
    mem_data = ""
    zf_data = ""
    pg_data = ""
    pa_data = ""
    db_data = ""
    dtrw_data = ""
    dtt_data = ""
    nio_data = ""
    np_data = ""
    ipc_data = ""
    ni_data = ""
    di_data = ""
    oc_data = ""
    pc_data = ""
    
    num_logical_cpus = 0
    while ('cpu' + str(num_logical_cpus)) in jdata[0]['stat']:
        num_logical_cpus += 1
        
    lcpu_data = []
    for i in range(num_logical_cpus):
        lcpu_data.append("")

    for i, s in enumerate(jdata):
        if(i == 0):
            continue
        
        # total CPU graph does not make much sense for a container -- we need to plot only container's allowed CPUs
        # cpu_data += ",['Date(%s)']\n" % (googledate(s['timestamp']['datetime']))
#         mhz_data += ",['Date(%s)', %.1f,%.1f]\n" % \
#                 ( googledate(s['timestamp']['datetime']), 
#                   s['lscpu']['cpu_max_mhz'], 
#                   s['lscpu']['cpu_mhz'] )
#         oc_data += ",['Date(%s)', %.1f]\n" %(googledate(s['timestamp']['datetime']), s['cpu_util']['freq_pct'])
    
#         pc_data += ",['Date(%s)', %.1f, %.1f, %.1f, %.1f, %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']),
#                 s['lpar_format2']['entitled_capacity'], 
#                 s['lpar_format2']['max_pool_capacity'], 
#                 s['lpar_format2']['entitled_pool_capacity'], 
#                 s['lpar_format2']['pool_busy_time'] / 100.0, 
#                 s['lpar_format2']['pool_scaled_busy_time'] / 100.0, 
#                 s['cpu_util']['physical_consumed'])
#     
#         pcpu_data += ",['Date(%s)', %d, %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
#                 s['total_physical_cpu']['user'], 
#                 s['total_physical_cpu']['sys'], 
#                 s['total_physical_cpu']['wait'], 
#                 s['total_physical_cpu']['idle'])
#     
        for i in range(num_logical_cpus):
            cpu_stats = s['stat']['cpu' + str(i)]
            lcpu_data[i] += "['Date(%s)', %d, %d, %d],\n" % (
                    googledate(s['timestamp']['datetime']),
                    cpu_stats['user'],
                    cpu_stats['sys'],
                    cpu_stats['idle']
                )
#     
#         try:
#             rq = s['kernel']['run_queue']
#         except:
#             rq = s['kernel']['runqueue']
#         rq_data += ",['Date(%s)', %.1f]\n" %(googledate(s['timestamp']['datetime']),rq)
#     
#         sc_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['kernel']['syscall'])
#     
#         ps_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['kernel']['pswitch'])
#     
#         rw_data += ",['Date(%s)', %d,%d]\n" %(googledate(s['timestamp']['datetime']),
#                 s['kernel']['sysread'], s['kernel']['syswrite'])
#     
#         di_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['kernel']['decrintrs'])
#     
#         rwmb_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['kernel']['readch']/1024/1024, s['kernel']['writech']/1024/1024)
#     
#         fe_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['kernel']['sysexec'], s['kernel']['sysexec'])
#     
#         ds_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['kernel']['devintrs'], s['kernel']['softintrs'])
#     
#         la_data += ",['Date(%s)', %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
#                     s['kernel']['load_avg_1_min'],
#                     s['kernel']['load_avg_5_min'],
#                     s['kernel']['load_avg_15_min'])
#     
#         mem_data += ",['Date(%s)', %.1f,%.1f, %.1f,%.1f, %.1f,%.1f, %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['memory']['real_total']/1024,
#                     s['memory']['real_free']/1024, 
#                     s['memory']['real_pinned']/1024,
#                     s['memory']['real_inuse']/1024, 
#                     s['memory']['real_system']/1024,
#                     s['memory']['real_user']/1024, 
#                     s['memory']['real_process']/1024,
#                     s['memory']['real_avail']/1024 )
#     
#         zf_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['vminfo']['zerofills'])
#     
#         pg_data += ",['Date(%s)', %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
#                     s['memory']['pgsp_total'],
#                     s['memory']['pgsp_free'],
#                     s['memory']['pgsp_rsvd'])
#     
#         pa_data += ",['Date(%s)', %.1f,%.1f, %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['memory']['pgins'],
#                     s['memory']['pgouts'],
#                     s['memory']['pgspins'],
#                     s['memory']['pgspouts'])
#     
#         ipc_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['kernel']['sema'], s['kernel']['msg'])
#     
#         ni_data += ",['Date(%s)', %.1f,%.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
#                     s['kernel']['namei'],
#                     s['kernel']['iget'],
#                     s['kernel']['dirblk'])
#     
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
    nchart_start_script(web, 'Monitoring data' + details)
    
    # ----- add graphs 
    for i in range(num_logical_cpus):
        graphit(web, "'User','System','Idle'", lcpu_data[i], 'Logical CPU ' + str(i) + details + " (from baremetal stats)", "cpu" + str(i), stacked)
    
    # if process_data_found:
    #    bubbleit(web, topprocs_title, topprocs,  'Top Processes Summary' + details, "TopSum")
    #    graphit(web, top_header, top_data,  'Top Procs by CPU time' + details, "TopProcs",unstacked)
    
    # graphit(web, td_header, td_data,  'Top Disks (mbps)' + details, "TopDisks",unstacked)
    # graphit(web, "'VP', 'Entitled', 'Busy', 'Consumed'", cpu_data,  'CPU cores' + details, "CPU Core",unstacked)
    # graphit(web, "'MHz Nominal', 'MHz Current'", mhz_data,  'CPU MHz' + details, "CPU MHz",unstacked)
    # graphit(web, "'Overclock'", oc_data,  'Frequency percent of Nominal MHz' + details, "Overclock",unstacked)
    # graphit(web, "'Entitled','Pool Size','Assigned to LPARs','Busy', 'Scaled Busy', 'Consumed'", pc_data,  'CPU Pool' + details, "CPU Pool",unstacked)
#     graphit(web, "'User','System','Wait','Idle'", pcpu_data,  'Total Physical CPU (stacked)' + details, "Pcpu",stacked)
#     graphit(web, "'Run Queue'", rq_data,  'CPU Run Queue' + details, "RunQueue",unstacked)
#     graphit(web, "'1_min_LoadAvg', '5_min_LoadAvg','15_min_LoadAvg'", la_data,  'Load Average' + details, "LoadAvg",unstacked)
#     graphit(web, "'System Calls'", sc_data,  'System Calls' + details, "SysCalls",unstacked)
#     graphit(web, "'Process Switch'", ps_data,  'Process Switches' + details, "pSwitch",unstacked)
#     graphit(web, "'Reads', 'Writes'", rw_data,  'Systems Calls Read & Write' + details, "SysCall-RW",unstacked)
#     graphit(web, "'Reads-MB', 'Writes-MB'", rw_data,  'Systems Calls Read MB & Write MB' + details, "SysCall-RWMB",unstacked)
#     graphit(web, "'Fork', 'Exec'", fe_data,  'Systems Calls fork() & exec()' + details, "Fork-Exec",unstacked)
#     graphit(web, "'Device', 'Soft'", ds_data,  'Interrupts by HW Device & Soft' + details, "Interrupts",unstacked)
#     graphit(web, "'Decr-Interrupts'", di_data,  'Decrementor Interrupts' + details, "DecIntr",unstacked)
#     graphit(web, "'Memory Size', 'Memory Free', 'Pinned', 'inuse', 'system', 'user', 'process', 'avail'", mem_data, 'Memory MB' + details, "Memory",unstacked)
#     graphit(web, "'zero fills'", zf_data,  'Memory Page Zero Fill' + details, "MemoryZero",unstacked)
#     graphit(web, "'Size', 'Free', 'Reserved'", pg_data,  'Paging Space in MB' + details, "PageSpace",unstacked)
#     graphit(web, "'PgIn', 'PgOut', 'PgSpaceIn','PgSpaceOut'", pa_data,  'Paging Filesystem(in & out) + Paging Space(in & out)' + details, "Paging",unstacked)
#     graphit(web, "'Read', 'Write'", dtrw_data,  'Disk Total Read & Write MB/s' + details, "TotalDisk-MB",unstacked)
#     graphit(web, "'Disk Transfer'", dtt_data,  'Disk total transfers/s' + details, "TotalDisk-xfer",unstacked)
#     graphit(web, "'namei','iget','dirblk'", ni_data,  'Directory lookup' + details, "Namei",unstacked)
#     graphit(web, "'Incoming', 'Outgoing'", nio_data,  'Network Bytes/s' + details, "TotalNet-Bytes",unstacked)
#     graphit(web, "'Incoming', 'Outgoing'", np_data,  'Network Packets/s' + details, "TotalNet-Xfer",unstacked)
#     graphit(web, "'semaphore','messages'", ipc_data,  'Inter-process commnunication' + details, "IPC",unstacked)
    
    # web.write(generate_disks(jdata))
    # generate_filesystems(web, jdata)
    # generate_network_traffic(web,jdata)
    
    # web.write(config_button_str)
    jdata_first_sample = jdata[0]
    monitoring_summary = [
    "Monitoring launched as: " + jdata_first_sample["njmon"]["njmon_command"],
    "njmon-cgroup-aware: " + jdata_first_sample["njmon"]["njmon_version"],
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
    
    nchart_end_script(web, generate_config(jdata_first_sample))
    nchart_start_body(web, hostname, buttonlist)
    nchart_append_table(web, "Monitoring Summary", monitoring_summary)
    nchart_append_table(web, "Monitored System Summary", monitored_summary)
    web.write('<p>NOTE: to zoom use left-click and drag; to reset view use right-click.</p>\n')
    nchart_end_body(web)
    web.close()
    print("Completed processing")

# The End

# MAIN


if __name__ == '__main__':
    cmd = sys.argv[0]
    infile = sys.argv[1]
    try:
        outfile = sys.argv[2]
    except:
        outfile = infile[:-4] + "html"

    main_process_file(cmd, infile, outfile)
