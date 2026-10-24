// HostClock: a transport kept in ChucK, driven by messages from a Pd patch.
// same API as chuck-max's built-in HostClock; see docs/dev/syncing.md.
// load it with '@import "HostClock.ck"' (pd-chuck only: chuck-max binds its
// own HostClock, and a second definition does not compile)
//
// Pd -> [chuck~], changes only:
//   msg clock tempo <bpm>        bpm > 0
//   msg clock play <0|1>         stop / start
//   msg clock seek <ticks>       480 ticks per quarter note
//   msg clock sig <num> <denom>  time signature
//
// broadcasts on every change, as the Max class does. a clock stops
// following the patch when the shred that created it ends

public class HostClock extends Event
{
    // ticks per quarter note, as Max counts them
    480.0 => static float RESOLUTION;

    // state at the last change any HostClock saw, so a clock created later
    // starts from it; the patch sends changes only
    static float s_ticks;
    static time s_t0;
    120.0 => static float s_tempo;
    static int s_running;
    4 => static int s_num;
    4 => static int s_denom;

    // the transport: position m_ticks at time m_t0
    s_ticks => float m_ticks;
    s_t0 => time m_t0;
    s_tempo => float m_tempo;
    s_running => int m_running;
    s_num => int m_num;
    s_denom => int m_denom;

    HostIn m_in;
    m_in.addAddress( "clock" );
    spork ~ listen();

    // --- queries, as chuck-max's HostClock ---------------------------------
    // transport position at now, in ticks
    fun float ticks() { return ticksAt( now ); }
    // tempo in BPM
    fun float tempo() { return m_tempo; }
    // 1 if the transport is running
    fun int running() { return m_running; }
    // duration of one beat at the current tempo
    fun dur beat() { return ( beatTicks() / tps() )::samp; }
    // time to the next beat boundary; never zero
    fun dur toNextBeat() { return toNext( beatTicks() ); }

    // time to the next multiple of 'period' ticks; never zero, so
    // 'clk.toNext(p) => now' in a loop cannot stall at a boundary. a stopped
    // transport answers as if it started now
    fun dur toNext( float period )
    {
        if( period <= 0 ) return 1::samp;
        ticks() => float t;
        ( Math.floor( t / period ) + 1 ) * period => float next;
        ( next - t ) / tps() => float samples;
        if( samples < 0.5 ) period / tps() +=> samples;
        return samples::samp;
    }

    // bar at now, from 1
    fun int bar() { return Math.floor( ticks() / barTicks() ) $ int + 1; }
    // beat within the bar at now, from 1
    fun int beatInBar() { return Math.floor( inBar() / beatTicks() ) $ int + 1; }
    // ticks into the current beat
    fun float unit() { return inBar() - ( beatInBar() - 1 ) * beatTicks(); }

    // --- internals ---------------------------------------------------------
    fun float tps() { return m_tempo / 60.0 * RESOLUTION * ( samp / second ); }
    fun float beatTicks() { return RESOLUTION * 4.0 / m_denom; }
    fun float barTicks() { return beatTicks() * m_num; }
    fun float inBar()
    {
        ticks() => float t;
        return t - Math.floor( t / barTicks() ) * barTicks();
    }

    fun float ticksAt( time t )
    {
        if( !m_running ) return m_ticks;
        return m_ticks + ( t - m_t0 ) / samp * tps();
    }

    // move the anchor to now, so a change applies from here on
    fun void anchor()
    {
        ticks() => m_ticks;
        now => m_t0;
    }

    // apply one message; returns 1 if the transport changed
    fun int apply( HostMsg m )
    {
        m.getString( 0 ) => string verb;
        if( verb == "tempo" )
        {
            m.getFloat( 1 ) => float bpm;
            if( bpm <= 0 ) { warn( "tempo must be > 0" ); return 0; }
            if( bpm == m_tempo ) return 0;
            anchor();
            bpm => m_tempo;
            return 1;
        }
        if( verb == "play" )
        {
            ( m.getFloat( 1 ) != 0 ) => int run;
            if( run == m_running ) return 0;
            anchor();
            run => m_running;
            return 1;
        }
        if( verb == "seek" )
        {
            m.getFloat( 1 ) => float to;
            // jumps below half a sample are float noise, as in Max
            if( Math.fabs( to - ticks() ) / tps() < 0.5 ) return 0;
            to => m_ticks;
            now => m_t0;
            return 1;
        }
        if( verb == "sig" )
        {
            m.getInt( 1 ) => int num;
            m.getInt( 2 ) => int denom;
            if( num < 1 || denom < 1 ) { warn( "sig needs num, denom >= 1" ); return 0; }
            if( num == m_num && denom == m_denom ) return 0;
            anchor();
            num => m_num;
            denom => m_denom;
            return 1;
        }
        warn( "unknown message '" + verb + "'" );
        return 0;
    }

    fun void warn( string what ) { cherr <= "HostClock: " + what <= IO.nl(); }

    fun void listen()
    {
        HostMsg m;
        while( true )
        {
            m_in => now;
            0 => int changed;
            while( m_in.recv( m ) ) if( apply( m ) ) 1 => changed;
            if( !changed ) continue;
            m_ticks => s_ticks;
            m_t0 => s_t0;
            m_tempo => s_tempo;
            m_running => s_running;
            m_num => s_num;
            m_denom => s_denom;
            this.broadcast();
        }
    }
}
