#ifndef SYNCSTATS_H
#define SYNCSTATS_H

#define SYNCSTATS_EWMA_ALPHA .2

class syncStats{
 private:
    double meanInterCSTime;
    uint64_t lastSyncEnd;
    uint64_t failed;
    uint64_t succeeded;
    double meanSpecSuccessRate;
    
 public:
    syncStats(){
        meanInterCSTime=0;
        lastSyncEnd=0;
        succeeded=0;
        failed=0;
        meanSpecSuccessRate=1.0;
    }
    
    void endSync(uint64_t currentTime){
        meanInterCSTime = (1.0 - SYNCSTATS_EWMA_ALPHA)*meanInterCSTime + SYNCSTATS_EWMA_ALPHA*(currentTime-lastSyncEnd);
        lastSyncEnd=currentTime;
    }

    double getMeanInterCSTime(){
        return meanInterCSTime;
    }

    void specSucceeded(){
        succeeded++;
        meanSpecSuccessRate = SYNCSTATS_EWMA_ALPHA + meanSpecSuccessRate*(1.0-SYNCSTATS_EWMA_ALPHA);
    }

    void specFailed(){
        failed++;
        meanSpecSuccessRate = meanSpecSuccessRate*(1.0-SYNCSTATS_EWMA_ALPHA);
    }

    double specPercentageOfSuccess(){
        return meanSpecSuccessRate;
    }

    uint64_t getFailedCount(){
        return failed;
    }

    uint64_t getSucceededCount(){
        return succeeded;
    }


};

#endif
