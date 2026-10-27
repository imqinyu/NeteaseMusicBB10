#ifndef NM_SLEEP_TIMER_HPP
#define NM_SLEEP_TIMER_HPP

#include <QtGlobal>
#include <QList>

namespace nm {

// Pure timing policy; elapsed time is supplied by the controller's monotonic clock.
class NmSleepTimer
{
public:
    enum Action { None, Pause, Wait };
    NmSleepTimer() : m_active(false), m_wait(false), m_waiting(false), m_duration(0) {}

    bool start(int minutes, bool waitForEnd)
    {
        if (minutes < 1 || minutes > 1440)
            return false;
        m_duration = qint64(minutes) * 60000;
        m_active = true;
        m_wait = waitForEnd;
        m_waiting = false;
        return true;
    }
    void cancel() { m_active = false; m_waiting = false; }
    bool active() const { return m_active; }
    bool waiting() const { return m_waiting; }
    bool waitForEnd() const { return m_wait; }
    qint64 remainingMs(qint64 elapsed) const
    {
        return m_active ? qMax(qint64(0), m_duration - elapsed) : 0;
    }
    int remaining(qint64 elapsed) const
    {
        return m_active ? int(qMax(qint64(0), m_duration - elapsed + 59999) / 60000) : 0;
    }
    Action tick(qint64 elapsed, bool canFinish)
    {
        if (!m_active || m_waiting || elapsed < m_duration)
            return None;
        if (m_wait && canFinish) {
            m_waiting = true;
            return Wait;
        }
        cancel();
        return Pause;
    }
    bool interceptCompletion(qint64 elapsed)
    {
        if (!m_active || (!m_waiting && elapsed < m_duration))
            return false;
        cancel();
        return true;
    }

    struct Projection {
        int index;
        qint64 extra;
        Projection(int i = -1, qint64 e = 0) : index(i), extra(e) {}
    };
    // Predict without changing the queue or consuming random numbers. An exact
    // boundary belongs to the track ending there, as in interceptCompletion().
    static Projection project(const QList<qint64> &durations, int current,
                              qint64 position, qint64 delay, bool repeatOne, bool shuffle)
    {
        if (current < 0 || current >= durations.size() || durations.at(current) <= 0)
            return Projection();
        const qint64 left = qMax(qint64(0), durations.at(current) - position);
        if (delay <= left)
            return Projection(current, left - delay);
        delay -= left;
        if (repeatOne) {
            const qint64 duration = durations.at(current);
            return Projection(current, (duration - delay % duration) % duration);
        }
        if (shuffle)
            return Projection();
        qint64 cycle = 0;
        for (int i = 0; i < durations.size(); ++i) {
            if (durations.at(i) <= 0)
                return Projection();
            cycle += durations.at(i);
        }
        delay = (delay - 1) % cycle + 1;
        for (int step = 1; step <= durations.size(); ++step) {
            const int i = (current + step) % durations.size();
            if (delay <= durations.at(i))
                return Projection(i, durations.at(i) - delay);
            delay -= durations.at(i);
        }
        return Projection();
    }

private:
    bool m_active;
    bool m_wait;
    bool m_waiting;
    qint64 m_duration;
};
}
#endif
