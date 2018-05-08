import os
import time
import datetime
import sched
import threading

from flask import Flask
app = Flask(__name__)
gData = ""
gDataLock = threading.Lock()

@app.route("/")
def index():
    return "Low latency docsis backend!"

@app.route("/data")
def data():
    data = ""
    global gData, gDataLock
    gDataLock.acquire()
    data = gData
    gDataLock.release()
    return data

class PeriodScheduler():
    def __init__(self):
        self.__sched = sched.scheduler(time.time, time.sleep)

    def DisplayTimeStamp(self):
        print datetime.datetime.fromtimestamp(time.time()).strftime('%Y%m%d-%H%M%S')

    # This api guarantee func will be invoked on the coming time point which is divisible by the period.
    def Enter(self, period, priority, func, args):
        now = int(time.time())
        if period == 0:
            time_point = now
        else:
            time_point = (now/period + 1) * period
        self.__sched.enterabs(time_point, priority, func, args)

    # This api guarantee func will be invoked exactly on the time point.
    def EnterTimePoint(self, time_point, priority, func, args):
        self.__sched.enterabs(time_point, priority, func, args)

    def Run(self):
        self.__sched.run()

class MeatureLatency():
    def __init__(self, id, server_port):
        self.__sched = PeriodScheduler()
        self.__backend_dir = "/root/lld/backend/"
        self.__input = "data%d.txt" %id
        self.__ouput = "output%d.txt" %id
        self.__server_port = server_port
        
    def __DealingData(self):
        datas = []
        label = "ms" # "duration"
        ifile = open(self.__backend_dir + self.__input, 'r')
        lines = ifile.readlines()
        for line in lines:
            if label in line:
                elements = line.split()
                datas.append(elements[0].split(":")[-1])
                val_in_ms = elements[-1]
                val_in_us = reduce(lambda x, y: x + y,  val_in_ms.split('.'))
                datas[-1] += " " + val_in_us + '\n'
        
        # ouput the data to frontend
        ofile = open(self.__backend_dir + self.__ouput, 'w')
        ofile.writelines(datas)
        ofile.write("END\n")
        ofile.close()
        
        data = "<pre>"
        data += reduce(lambda x, y: x+y, lines) 
        data += "</pre>"
        global gData, gDataLock
        gDataLock.acquire()
        gData = data
        gDataLock.release()
        
    def __PortOwtraffic(self):
        self.__sched.DisplayTimeStamp()
        os.system("cd " + self.__backend_dir + "; /root/lld/owtraffic/owtraffic --server-port %d --output-file %s" %(self.__server_port, self.__backend_dir + self.__input))
        self.__DealingData()
        self.__sched.Enter(10, 0, MeatureLatency.__PortOwtraffic,[self])
        self.__sched.DisplayTimeStamp()

    def Run(self):
        self.__PortOwtraffic()
        self.__sched.Run()

if __name__ == "__main__":
    tasks = [(1, 44444), (2, 44443)]
    for task in tasks: 
        meature_latency = MeatureLatency(task[0], task[1])
        ow_thread = threading.Thread(target = MeatureLatency.Run, args=[meature_latency])
        ow_thread.start()
    app.run('0.0.0.0')

