from random import randint
import os
def outputing(algo:str,no_of_dist_prog:int):
    ofile = open("{}-stats.txt".format(algo), "r")
    lines = ofile.readlines()
    ofile.close()

    dead_process = 0

    for word in lines[2].split():
        if word.isdigit():
            dead_process = int(word)

    avg_sum = 0
    for i in range(no_of_dist_prog):
        num = 0
        for word in lines[4+i].split():
            if word.replace('.','1').isdigit():
                num += 1
                if num == 2:
                    avg_sum += float(word)
    global deadline_list,avg_list

    k = 0
    if algo =="EDF":
        k = 1

    deadline_list[k][no_of_dist_prog-2] = dead_process
    avg_list[k][no_of_dist_prog-2] = int(avg_sum/no_of_dist_prog)
    



deadline_list = [[0,0,0,0,0,0,0,0,0],[ 0,0,0,0,0,0,0,0,0]]
avg_list = [[0, 0, 0, 0, 0, 0, 0, 0, 0],[0, 0, 0, 0, 0, 0, 0, 0, 0]]

for j in range(2,11):
    no_of_dist_prog = j
    ifile = open("inp-params.txt","r")
    input_lines = ifile.readlines()
    ifile.close()

    ifile = open("inp-params.txt","w")
    
    ifile.write("{}\n".format(j))

    for i in range(1,no_of_dist_prog):
        ifile.write(input_lines[i])

    ifile.write("{} ".format(j))
    t = randint(1, 100)
    ifile.write("{} ".format(t))
    p = int(1*t +t*(1+randint(-5,5)/10))
    # p = randint(t,100)
    ifile.write("{} ".format(p))
    ifile.write("{}\n".format(10))



    ifile.close()

    os.system("./RMS")
    os.system("./EDF")

    outputing("RMS",j)
    outputing("EDF",j)

print(deadline_list)
print(avg_list)

import numpy as np
import matplotlib.pyplot as plt

no_of_processes = [20,30,40,50,60,70,80,90,100]

plt.xlabel("Total no of Processes")
plt.ylabel("No. of processes missed deadline")
plt.title("Deadline missed vs Number of processes:")
plt.plot(no_of_processes, deadline_list[0], marker="o", markersize=8, markerfacecolor="black",
         markeredgecolor="black", c="green", label="EDF")
plt.plot(no_of_processes, deadline_list[1], marker="o", markersize=8, markerfacecolor="black",
         markeredgecolor="black", c="red", label="RMS")
plt.legend(loc="lower right")
plt.show()


plt.xlabel("Total no of Processes")
plt.ylabel("Average Waiting Time")
plt.title("Average Waiting time vs Number of processes:")
plt.plot(no_of_processes, avg_list[0], marker="o", markersize=8, markerfacecolor="black",
         markeredgecolor="black", c="green", label="EDF")
plt.plot(no_of_processes, avg_list[1] , marker="o", markersize=8, markerfacecolor="black",
         markeredgecolor="black", c="red", label="RMS")
plt.legend(loc="lower right")
plt.show()
