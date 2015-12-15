from Tkinter import Tk, Canvas
import Image, ImageDraw, ImageFont
import time
import sys, getopt


our_width = 65000
our_height = 2600

#top = Tk()
#canvas = Canvas(top, width=our_width, height=our_height)
#canvas.pack()


image1 = Image.new("RGB", (our_width, our_height), "white")
draw = ImageDraw.Draw(image1)

#red = transaction........
#blue = waiting for token.....
#green = conversion commit
#black = waiting to become the lowest
#purple = Lock failed...had to wait

colors = { "0" : "red" , "1" : "blue", "2" : "green", "3" : "black", "4" : "yellow", "5" : "purple" , "20" : "orange", "21" : "gray", "22" : "lime", "32" : "red", "33" : "blue", "34": "green", "7":"orange", "39" : "gray", "40" : "lime", "41" : "red", "42" : "blue", "43": "green"}

font = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf", 20)
fontSmall = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf", 16)
fontLarge = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf", 28)


for row in sys.stdin:
    event = row.strip("\n").split(" ")
    #thread_id,start_time,end_time,event_type,clock,clock_diff,c_p,u_p,p_p,m_p,sync,tx_counter,tx_level = int(event[0]) * 200 + 200, int(event[1]), int(event[2]), event[3], event[4], event[5], event[6], event[7], event[8], event[9], event[10], event[11], event[12]
    thread_id,start_time,end_time,event_type,begin_clock,end_clock = int(event[0]) * 200 + 200, int(event[1]), int(event[2]), event[3], int(event[4])/1000, int(event[4])/1000+int(event[5])/1000
    if (start_time > 0 and end_time < our_width):
        print str(thread_id) + " " + str(start_time) + " " + str(end_time) + " " + event_type
        draw.line([start_time,thread_id,end_time,thread_id], fill=colors[event_type], width=40)
        if ((end_time - start_time) > 80):
            draw.text((start_time, thread_id + 60), str(start_time), (0,0,0), font=fontLarge)
            draw.text((start_time, thread_id - 60), str(begin_clock)+","+str(end_clock), (0,0,0), font=fontSmall)
        #if ((end_time - start_time) > 50):
        #    draw.text((start_time,thread_id + 30), str(int(clock)/1000), (0,0,0), font=fontLarge)
        #    draw.text((start_time + 30,thread_id), clock_diff, (0,0,0), font=font)
        #if (event_type=="2"):
        #    draw.text((start_time, thread_id-45), c_p + "," + u_p + "," + p_p + "," + str(m_p), (0,0,0), font=fontSmall)
        if (event_type=="20" or event_type=="21" or event_type=="22" or int(event_type) > 30):
            draw.line([start_time,thread_id+80,start_time+20,thread_id+80], fill=colors[event_type], width=20)

print "hi"
filename = "thread_events_" + str(sys.argv[1]) + ".png";
print "hi 2"
image1.save(filename)
print "hi 3"
