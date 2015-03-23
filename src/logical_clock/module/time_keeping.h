
#ifndef TIME_KEEPING_H

#define TIME_KEEPING_H


int __lowest_time(struct timespec * t1, struct timespec * t2){
    if (t1->tv_sec != t2->tv_sec){
        return (t1->tv_sec < t2->tv_sec) ? 1 : 2;
    }
    else {
        return (t1->tv_nsec < t2->tv_nsec) ? 1 : 2;
    }
}


unsigned long __elapsed_time_ns(struct timespec * t1, struct timespec * t2){
    struct timespec * start, * end;
    int lowest = __lowest_time(t1,t2);
    start = (lowest==1) ? t1 : t2;
    end = (lowest==1) ? t2 : t1;
        
    return (end->tv_sec-start->tv_sec)*1000000000+(end->tv_nsec-start->tv_nsec);
}

unsigned long __elapsed_time_us(struct timespec * t1, struct timespec * t2){
    struct timespec * start, * end;
    int lowest = __lowest_time(t1,t2);
    start = (lowest==1) ? t1 : t2;
    end = (lowest==1) ? t2 : t1;
        
    return (end->tv_sec-start->tv_sec)*1000000UL+((end->tv_nsec-start->tv_nsec)/1000UL);
}





#endif
