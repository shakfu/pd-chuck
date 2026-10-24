// HostClock: click on each beat of a Pd transport, accent on beat 1.
// in Pd: [msg clock tempo 120, clock play 1( -> [chuck~]; also
// 'clock play 0', 'clock seek 0', 'clock sig 3 4'. the beat number goes
// to [r beat]. the loop below is chuck-max's max_clock.ck unchanged
@import "HostClock.ck"

HostClock clk;
Impulse imp => ResonZ r => dac;
20 => r.Q;

fun void click()
{
    clk.beatInBar() => int b;
    ( b == 1 ? 1760 : 880 ) => r.freq;
    5 => imp.next;
    Host.sendFloat( "beat", b );
}

fun void beats()
{
    // toNextBeat() is never zero, so a start exactly on a beat would skip it
    if( clk.unit() < 0.01 ) click();
    while( true )
    {
        clk.toNextBeat() => now;
        click();
    }
}

// restart on any transport change, so a wait computed at the old tempo
// never finishes late
Shred @ loop;
while( true )
{
    if( loop != null ) loop.exit();
    null @=> loop;
    if( clk.running() ) spork ~ beats() @=> loop;
    clk => now;
}
