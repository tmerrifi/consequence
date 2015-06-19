#ifndef SYNCSTATS_H
#define SYNCSTATS_H

#define SYNCSTATS_EWMA_ALPHA .3

class syncStats{
 private:
    double meanInterCSTime;
    uint64_t lastSyncEnd;

 public:
    syncStats(){
        meanInterCSTime=0;
        lastSyncEnd=0;
    }
    
    void endSync(uint64_t currentTime){
        meanInterCSTime = (1.0 - SYNCSTATS_EWMA_ALPHA)*meanInterCSTime + SYNCSTATS_EWMA_ALPHA*(currentTime-lastSyncEnd);
        lastSyncEnd=currentTime;
    }

    double getMeanInterCSTime(){
        return meanInterCSTime;
    }
};

#endif
