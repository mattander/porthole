#pragma once
#include <QtGlobal>

// One budget for both iCloud readers. Discovery alone cannot renew the budget:
// a server may accept discovery and still reject calendar REPORT access.
class IcloudRecovery {
    int rejections=0;
    qint64 nextAttempt=0;
public:
    static constexpr qint64 FirstDelayMs=5*60*1000;
    static constexpr qint64 LaterDelayMs=30*60*1000;
    bool recovering()const{return rejections>0;}
    bool paused()const{return rejections>=3;}
    bool ready(qint64 elapsed)const{return !paused()&&elapsed>=nextAttempt;}
    void rejected(qint64 elapsed){
        if(rejections<3)++rejections;
        nextAttempt=elapsed+(rejections==1?FirstDelayMs:LaterDelayMs);
    }
    // After exhaustion, an explicit user retry buys exactly one attempt, never
    // another automatic batch. Clicks and normal polls cannot bypass cooldown.
    void retry(qint64 elapsed){if(paused()&&elapsed>=nextAttempt)rejections=2;}
    void recovered(){rejections=0;nextAttempt=0;}
};
