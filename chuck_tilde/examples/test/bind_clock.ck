// run by tests/test_bind_clock.pd ('make test-bind'): HostClock.ck driven
// by timed 'msg clock ...' messages. reports 'msg pass <id>' or
// 'msg fail <id>' per check, then 'msg done <failures>'
@import "../pd/HostClock.ck"

0 => int failures;
fun void check( int ok, int id )
{
    if( ok ) Host.msg( "pass", id );
    else { Host.msg( "fail", id ); failures++; }
}
fun int near( float a, float b ) { return Math.fabs( a - b ) < 1e-6; }
fun int nearDur( dur a, dur b ) { return Math.fabs( ( a - b ) / samp ) < 1e-6; }

// ticks per second at 120 BPM, 480 ticks per quarter
960.0 => float tps120;

HostClock clk;
0 => int broadcasts;
fun void count() { while( true ) { clk => now; broadcasts++; } }
spork ~ count();

// defaults: 120 BPM, 4/4, stopped at 0
check( clk.tempo() == 120 && !clk.running() && clk.ticks() == 0, 1 );
check( clk.bar() == 1 && clk.beatInBar() == 1 && clk.unit() == 0, 2 );
check( nearDur( clk.beat(), 0.5::second ), 3 );
// stopped: answers as if started now
check( nearDur( clk.toNextBeat(), 0.5::second ), 4 );

// 'tempo 120' (no change), 'play 1'
clk => now;
check( clk.running() && clk.ticks() == 0, 5 );
check( nearDur( clk.toNextBeat(), 0.5::second ), 6 );
clk.toNextBeat() => now;
check( near( clk.ticks(), 480 ) && clk.beatInBar() == 2 && clk.bar() == 1, 7 );
// at a boundary, toNext is a full period, never zero
check( nearDur( clk.toNextBeat(), 0.5::second ), 8 );
check( nearDur( clk.toNext( 120 ), 0.125::second ), 9 );

// 'seek 1920': bar 2 in 4/4
clk => now;
now => time ts;
check( clk.ticks() == 1920 && clk.bar() == 2 && clk.beatInBar() == 1, 10 );

// 'sig 3 4', about 200 ms later: position keeps running
clk => now;
1920 + ( now - ts ) / second * tps120 => float expect;
check( near( clk.ticks(), expect ), 11 );
// about 2112 ticks: bar 2 of 3/4 starts at 1440, beat 2 at 1920
check( clk.bar() == 2 && clk.beatInBar() == 2, 12 );

// 'tempo 60': position is continuous across the change
clk => now;
now => time tc;
1920 + ( tc - ts ) / second * tps120 => float atChange;
check( near( clk.ticks(), atChange ) && clk.tempo() == 60, 13 );
check( nearDur( clk.beat(), 1::second ), 14 );
100::ms => now;
check( near( clk.ticks(), atChange + 48 ), 15 );

// a clock created now starts from the last change
HostClock late;
check( late.tempo() == 60 && late.running() && near( late.ticks(), clk.ticks() ), 16 );

// second 'tempo 60' is no change; 'play 0' stops
clk => now;
clk.ticks() => float frozen;
check( !clk.running() && !late.running(), 17 );
100::ms => now;
check( clk.ticks() == frozen && near( late.ticks(), frozen ), 18 );

// 'bogus', 'tempo -5', 'sig 0 4' are rejected without a broadcast
300::ms => now;
check( clk.tempo() == 60 && !clk.running(), 19 );
// play 1, seek, sig, tempo 60, play 0
check( broadcasts == 5, 20 );

Host.msg( "done", failures );
