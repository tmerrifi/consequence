#ifndef SYNCSTATS_H
#define SYNCSTATS_H

#define SYNCSTATS_EWMA_ALPHA .3

class syncStats{
 private:
    double meanInterCSTime;
    uint64_t lastSyncEnd;
    uint64_t specFailed;
    uint64_t specTotal;
    
 public:
    syncStats(){
        meanInterCSTime=0;
        lastSyncEnd=0;
        specTotal=10;
        specFailed=1;
    }
    
    void endSync(uint64_t currentTime){
        meanInterCSTime = (1.0 - SYNCSTATS_EWMA_ALPHA)*meanInterCSTime + SYNCSTATS_EWMA_ALPHA*(currentTime-lastSyncEnd);
        lastSyncEnd=currentTime;
    }

    double getMeanInterCSTime(){
        return meanInterCSTime;
    }

    void specInc(){
        specTotal++;
    }

    void specFailedInc(){
        specFailed++;
    }

    double specPercentageOfFailure(){
        return (double)specFailed/(double)specTotal;
    }
};

#endif
