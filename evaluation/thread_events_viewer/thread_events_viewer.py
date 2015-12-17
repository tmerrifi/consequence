from Tkinter import Tk, Canvas
import Image, ImageDraw, ImageFont
import time
import random
import sys, getopt


our_width = int(sys.argv[2]) + 200
our_height = 2600

image1 = Image.new("RGB", (our_width, our_height), "white")

draw = ImageDraw.Draw(image1)

#enum debug_event_type{
#    DEBUG_TYPE_TRANSACTION = 0, DEBUG_TYPE_TOKEN_WAIT=1, DEBUG_TYPE_COMMIT=2, 
#    DEBUG_TYPE_WAIT_LOWEST=3, DEBUG_TYPE_BARRIER_WAIT=4, DEBUG_TYPE_LOCK_FAILED=5, DEBUG_TYPE_WAIT_ON_COND=6, DEBUG_TYPE_LIB=7, DEBUG_TYPE_FORK=8,
#    DEBUG_TYPE_COND_SIG=20, DEBUG_TYPE_COND_WAIT=21, DEBUG_TYPE_COND_WOKE_UP=22,
#    DEBUG_TYPE_MUTEX_LOCK=23, DEBUG_TYPE_MUTEX_UNLOCK=24, DEBUG_TYPE_TOKEN_FAILED=25, DEBUG_TYPE_LOCK_SPIN_WAKE=26, DEBUG_TYPE_LOCK_CONDVAR_WAKE=27,
#    DEBUG_TYPE_TX_COARSE_SUCCESS=28, DEBUG_TYPE_TX_COARSE_FAILED=29, DEBUG_TYPE_TX_ENDING=30, DEBUG_TYPE_TX_START=31, DEBUG_TYPE_MALLOC=32,
#    DEBUG_TYPE_STOP_CLOCK_NOC=33, DEBUG_TYPE_STOP_CLOCK=34, DEBUG_TYPE_START_CLOCK_NOC=35, DEBUG_TYPE_START_CLOCK=36, DEBUG_TYPE_START_COARSE=37, DEBUG_TYPE_END_COARSE=38,
#  DEBUG_TYPE_BEGIN_SPECULATION=39,DEBUG_TYPE_FAILED_SPECULATION=40,DEBUG_TYPE_END_SPECULATION=41,DEBUG_TYPE_SPECULATIVE_LOCK=42,DEBUG_TYPE_SPECULATIVE_UNLOCK=43,
#   DEBUG_TYPE_SPECULATIVE_COMMIT=44,DEBUG_TYPE_SPECULATIVE_NOSPEC=45,DEBUG_TYPE_SPECULATIVE_VALIDATE_OR_ROLLBACK=46,DEBUG_TYPE_SPECULATIVE_CURRENT_TICKS=47,
#    DEBUG_TYPE_SPECULATIVE_MAX_TICKS=48, DEBUG_TYPE_SPECULATIVE_SHOULDSPEC=49
    #};


eventKey = { "0" : "TRANSACTION", "1" : "TOKEN_WAIT", "2" : "CONVERSION_COMMIT", "3" : "WAIT_LOWEST", "20" : "COND_SIG", "21" : "COND_WAIT",
             "23" : "MUTEX_LOCK", "24" : "MUTEX_UNLOCK", "39" : "BEGIN_SPECULATION", "40" : "FAILED_SPECULATION", "41" : "END_SPECULATION",
             "32" : "MALLOC", "50" : "FREE", "51" : "COND_INIT", "52" : "MUTEX_INIT"};

colors = { "TRANSACTION" : "red", "TOKEN_WAIT" : "blue", "CONVERSION_COMMIT" : "green", "WAIT_LOWEST" : "black" };

#colors = { "0" : "red" , "1" : "blue", "2" : "green", "3" : "black", "4" : "yellow", "5" : "purple" , "20" : "orange", "21" : "gray", "22" : "lime", "32" : "red", "33" : "blue", "34": "green", "7":"orange", "39" : "gray", "40" : "lime", "41" : "red", "42" : "blue", "43": "green"}

font = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf", 20)
fontSmall = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf", 14)
fontLarge = ImageFont.truetype("/usr/share/fonts/truetype/liberation/LiberationSansNarrow-Bold.ttf", 28)

counter = 0

for row in sys.stdin:
    if counter % 2 == 0:
        offset=30
    else:
        offset=-40
    
    event = row.strip("\n").split(" ")
    thread_id,start_time,end_time,event_type,begin_clock,end_clock = int(event[0]) * 200 + 200, int(event[1]), int(event[2]), event[3], int(event[4])/1000, int(event[4])/1000+int(event[5])/1000
    if (start_time > 0 and end_time < our_width and event_type in eventKey):
        counter+=1
        if eventKey[event_type]=="COND_SIG":
            draw.text((start_time,thread_id + offset), "CS", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="COND_WAIT":
            draw.text((start_time,thread_id + offset), "CW", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="MUTEX_LOCK":
            draw.text((start_time,thread_id + offset), "L", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="MUTEX_UNLOCK":
            draw.text((start_time,thread_id + offset), "U", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="BEGIN_SPECULATION":
            draw.text((start_time,thread_id + offset), "BTx", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="FAILED_SPECULATION":
            draw.text((start_time,thread_id + offset), "FTx", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="END_SPECULATION":
            draw.text((start_time,thread_id + offset), "ETx", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="MALLOC":
            draw.text((start_time,thread_id + offset), "M", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="FREE":
            draw.text((start_time,thread_id + offset), "F", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="COND_INIT":
            draw.text((start_time,thread_id + offset), "CI", (0,0,0), font=fontSmall)
        elif eventKey[event_type]=="MUTEX_INIT":
            draw.text((start_time,thread_id + offset), "MI", (0,0,0), font=fontSmall)
            
        else:
            draw.line([start_time,thread_id,end_time,thread_id], fill=colors[eventKey[event_type]], width=40)

filename = str(sys.argv[1]) + ".png";
image1.save(filename)
