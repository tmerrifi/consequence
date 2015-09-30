
#ifndef LOGICAL_CLOCK_H
#define LOGICAL_CLOCK_H

#ifdef TOKEN_ORDER_ROUND_ROBIN

#define LOGICAL_CLOCK_TIME_LOCK 0

#define LOGICAL_CLOCK_TIME_UNLOCK 0

#define LOGICAL_CLOCK_TIME_FORK_PER_PAGE 0

#define LOGICAL_CLOCK_ROUND_ROBIN_INFINITY 50000  

//we need to make sure when a thread is forked its the next to get
//the token...not the parent
#define LOGICAL_CLOCK_ROUND_ROBIN_FORKED_THREAD 10

#define LOGICAL_CLOCK_CONVERSION_COMMIT_PAGE 0

#define LOGICAL_CLOCK_CONVERSION_UPDATE_PAGE 0

#define LOGICAL_CLOCK_CONVERSION_MERGE_PAGE 0

#define LOGICAL_CLOCK_CONVERSION_COW_PF 0

#else

#define LOGICAL_CLOCK_TIME_LOCK 0

#define LOGICAL_CLOCK_TIME_UNLOCK 0

#define LOGICAL_CLOCK_TIME_FORK_PER_PAGE 0 //200

#define LOGICAL_CLOCK_ROUND_ROBIN_INFINITY 0

//#define LOGICAL_CLOCK_CONVERSION_COMMIT_PAGE 4000

//#define LOGICAL_CLOCK_CONVERSION_UPDATE_PAGE 2000

//#define LOGICAL_CLOCK_CONVERSION_MERGE_PAGE 8000


#define LOGICAL_CLOCK_CONVERSION_COMMIT_PAGE 0

#define LOGICAL_CLOCK_CONVERSION_UPDATE_PAGE 0

#define LOGICAL_CLOCK_CONVERSION_MERGE_PAGE 0

#define LOGICAL_CLOCK_CONVERSION_COW_PF 500

#endif

#ifdef STATIC_TX_SIZE
#define LOGICAL_CLOCK_MIN_ALLOWABLE_TX_SIZE STATIC_TX_SIZE
#define LOGICAL_CLOCK_TX_SIZE_AFTER_TOKEN_TRANSFER STATIC_TX_SIZE
#define LOGICAL_CLOCK_MAX_ALLOWABLE_TX_SIZE STATIC_TX_SIZE
#define LOGICAL_CLOCK_ALLOWABLE_TX_INC 1000000
#else
#define LOGICAL_CLOCK_MIN_ALLOWABLE_TX_SIZE 20000
#define LOGICAL_CLOCK_TX_SIZE_AFTER_TOKEN_TRANSFER 50000
#define LOGICAL_CLOCK_MAX_ALLOWABLE_TX_SIZE 500000
#define LOGICAL_CLOCK_ALLOWABLE_TX_INC 50000
#endif


#endif
