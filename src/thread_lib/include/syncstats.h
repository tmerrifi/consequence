#ifndef SYNCSTATS_H
#define SYNCSTATS_H

#define SYNCSTATS_EWMA_ALPHA 0.0

class syncStats{
 private:
    uint64_t failed;
    uint64_t succeeded;
    double meanSpecSuccessRate;
    
 public:
    void initSyncStats(){
        succeeded=0;
        failed=0;
        meanSpecSuccessRate=1.0;
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
        return (meanSpecSuccessRate*meanSpecSuccessRate*meanSpecSuccessRate);
    }

    uint64_t getFailedCount(){
        return failed;
    }

    uint64_t getSucceededCount(){
        return succeeded;
    }


};

#endif
