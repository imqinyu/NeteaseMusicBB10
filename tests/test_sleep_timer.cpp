#include "nm/util/NmSleepTimer.hpp"
#include <cstdio>
#include <cstdlib>

static int checks = 0;
static void check(bool ok, const char *message)
{
    ++checks;
    if (!ok) {
        std::printf("FAIL: %s\n", message);
        std::exit(1);
    }
}

int main()
{
    nm::NmSleepTimer timer;
    check(!timer.active() && timer.remaining(0) == 0, "initially inactive");
    check(!timer.start(0, false) && !timer.start(-1, false) && !timer.start(1441, false), "invalid duration rejected");
    check(timer.start(1, false), "start one minute");
    check(timer.remaining(59999) == 1, "round remaining minutes up");
    check(timer.tick(59999, true) == nm::NmSleepTimer::None, "do not fire early");
    check(timer.tick(60000, true) == nm::NmSleepTimer::Pause && !timer.active(), "pause exactly at deadline");
    check(timer.tick(60001, true) == nm::NmSleepTimer::None, "expiry fires only once");
    check(timer.start(2, false), "rearm");
    check(timer.tick(240000, false) == nm::NmSleepTimer::Pause, "delayed background tick catches deadline");
    timer.start(1, true);
    check(!timer.interceptCompletion(59000), "normal next/repeat before deadline");
    check(timer.tick(60000, true) == nm::NmSleepTimer::Wait, "wait for current playing/paused track");
    check(timer.active() && timer.waiting() && timer.remaining(60000) == 0, "waiting remains cancellable");
    check(timer.tick(90000, true) == nm::NmSleepTimer::None, "waiting does not repeatedly expire");
    check(timer.interceptCompletion(95000) && !timer.active(), "suppress next/repeat after waiting");
    check(!timer.interceptCompletion(95000), "completion consumed once");
    timer.start(1, true);
    check(timer.interceptCompletion(60000), "completion before timer tick at deadline is intercepted");
    timer.start(1, true);
    check(timer.tick(60000, false) == nm::NmSleepTimer::Pause, "do not wait forever when nothing is playing");
    timer.start(1, false);
    timer.cancel();
    check(!timer.active() && timer.tick(60000, true) == nm::NmSleepTimer::None, "cancel does not expire later");
    timer.start(1, true);
    timer.tick(60000, true);
    timer.cancel();
    check(!timer.waiting() && !timer.interceptCompletion(90000), "cancelling wait permits normal completion");
    timer.start(1, true);
    check(!timer.start(0, false) && timer.waitForEnd() && timer.active(), "invalid input preserves existing timer");
    check(timer.start(1440, false) && timer.remainingMs(0) == 86400000, "24 hours does not overflow");

    QList<qint64> durations;
    durations << 180000 << 240000 << 120000;
    nm::NmSleepTimer::Projection p = nm::NmSleepTimer::project(durations, 0, 60000, 60000, false, false);
    check(p.index == 0 && p.extra == 60000, "predict within current track");
    p = nm::NmSleepTimer::project(durations, 0, 60000, 120000, false, false);
    check(p.index == 0 && p.extra == 0, "exact current boundary belongs to ending track");
    p = nm::NmSleepTimer::project(durations, 0, 60000, 180000, false, false);
    check(p.index == 1 && p.extra == 180000, "predict next track and wait extension");
    p = nm::NmSleepTimer::project(durations, 0, 60000, 360000, false, false);
    check(p.index == 1 && p.extra == 0, "exact next boundary");
    p = nm::NmSleepTimer::project(durations, 2, 60000, 60000 + 540000LL * 10 + 1, false, false);
    check(p.index == 0 && p.extra == 179999, "wrap through multiple queue cycles");
    p = nm::NmSleepTimer::project(durations, 0, 60000, 300000, true, true);
    check(p.index == 0 && p.extra == 0, "single-track repeat overrides shuffle and handles boundary");
    p = nm::NmSleepTimer::project(durations, 0, 60000, 300001, true, false);
    check(p.index == 0 && p.extra == 179999, "repeat projects later cycles");
    check(nm::NmSleepTimer::project(durations, 0, 60000, 60000, false, true).index == 0, "shuffle still predicts current track");
    check(nm::NmSleepTimer::project(durations, 0, 60000, 120001, false, true).index == -1, "never invent shuffled final track");
    durations[1] = 0;
    check(nm::NmSleepTimer::project(durations, 0, 60000, 120001, false, false).index == -1, "unknown duration is not guessed");
    std::printf("PASS: %d sleep timer and queue projection checks\n", checks);
    return 0;
}
