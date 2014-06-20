#pragma once

struct Timer
{
    struct Owner
    {
        virtual void timerExpired ( Timer *timer ) {}
    };

    Owner *owner;

private:

    long delay, expiry;

public:

    Timer ( Owner *owner );
    ~Timer();

    void start ( long delay );

    inline bool isStarted() const { return ( delay >= 0 || expiry >= 0 ); }

    friend class EventManager;
};
