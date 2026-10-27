// run by tests/test_bind.pd ('make test-bind'). reports 'msg pass <id>' or
// 'msg fail <id>' per check; 'msg ready' and 'msg resize' ask the patch to
// act; 'msg done <failures>' ends the run

0 => int failures;
fun void check( int ok, int id )
{
    if( ok ) Host.msg( "pass", id );
    else { Host.msg( "fail", id ); failures++; }
}
fun int near( float a, float b ) { return Math.fabs( a - b ) < 1e-6; }

// --- HostBuffer: the patch holds [array define bt_arr 8] ---------------------
HostBuffer a;
check( a.name() == "" && !a.ready() && a.frames() == 0, 1 );
check( a.name( "bt_arr" ) == "bt_arr", 2 );
check( a.ready() && a.frames() == 8, 3 );
for( 0 => int i; i < 8; i++ ) a.poke( i, i * 0.5 );
check( near( a.peek( 3 ), 1.5 ) && near( a.peek( 7 ), 3.5 ), 4 );
check( a.peek( -1 ) == 0 && a.peek( 8 ) == 0, 5 );
a.poke( 8, 9.0 );
a.poke( -1, 9.0 );
check( near( a.peek( 7 ), 3.5 ) && near( a.peek( 0 ), 0 ), 6 );

HostBuffer missing;
missing.name( "bt_none" );
check( !missing.ready() && missing.frames() == 0 && missing.peek( 0 ) == 0, 7 );

// a second object on the same name shares the slot
HostBuffer a2;
a2.name( "bt_arr" );
check( near( a2.peek( 3 ), 1.5 ), 8 );

// --- HostBufPlay: exact output sequences ----------------------------------
HostBufPlay p => blackhole;
fun int plays( float expect[] )
{
    for( 0 => int i; i < expect.size(); i++ )
    {
        1::samp => now;
        if( !near( p.last(), expect[i] ) )
        {
            <<< "HostBufPlay: sample", i, "got", p.last(), "want", expect[i] >>>;
            return 0;
        }
    }
    return 1;
}
p.name( "bt_arr" );
check( p.ready() && p.frames() == 8, 9 );
0 => p.interp;
// one-shot, rate 1: the array, then silence
check( plays( [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 0.0, 0.0] ), 10 );
// loop, half rate, linear interpolation; wraps from 3.5 to 0 at the end
p.name( "bt_arr" );
1 => p.loop;
1 => p.interp;
0.5 => p.rate;
6 => p.pos;
check( plays( [3.0, 3.25, 3.5, 1.75, 0.0, 0.25] ), 11 );
// backward, no interpolation
0 => p.interp;
-1 => p.rate;
1 => p.pos;
check( plays( [0.5, 0.0, 3.5, 3.0] ), 12 );

HostBufPlay none => blackhole;
none.name( "bt_none" );
1::samp => now;
check( !none.ready() && none.last() == 0, 13 );

// --- members shared with HostBuffer / HostBufPlay in the other hosts -------
// one channel at the patch rate; 0 while not ready
check( a.channels() == 1 && near( a.sampleRate(), second / samp )
       && missing.channels() == 0 && missing.sampleRate() == 0, 25 );
// channel 0 is the array; any other reads 0 and ignores writes
a.poke( 2, 0, 7.0 );
a.poke( 2, 1, 9.0 );
check( near( a.peek( 2, 0 ), 7.0 ) && a.peek( 2, 1 ) == 0 && near( a.peek( 2 ), 7.0 ), 26 );
a.poke( 2, 1.0 );
// HostBufPlay: another channel is silent and holds the position
p.name( "bt_arr" );
0 => p.loop;
1 => p.rate;
// warns once per object: make test-bind counts one warning
2 => p.channel;
1 => p.channel;
check( p.channel() == 1 && plays( [0.0, 0.0, 0.0] ), 27 );
0 => p.channel;
check( plays( [0.0, 0.5, 1.0] ), 28 );

// --- arrays are re-read every block: follow a resize ----------------------
Host.msg( "resize", 16 );
now + 1::second => time limit;
while( a.frames() != 16 && now < limit ) 1::ms => now;
check( a.frames() == 16 && near( a.peek( 3 ), 1.5 ), 14 );

// --- HostIn / HostMsg ---------------------------------------------------------
HostIn pin;
pin.addAddress( "note" );
HostIn pin2;
pin2.addAddress( "note" );
pin2.addAddress( "cc" );
HostMsg m;
check( m.name() == "" && m.numArgs() == 0, 15 );

// the patch answers with: msg other 1, msg note 60 0.8 legato, msg note 61,
// msg cc 7
Host.msg( "ready", 1 );
pin => now;
check( pin.recv( m ) && m.name() == "note" && m.numArgs() == 3, 16 );
check( m.getInt( 0 ) == 60 && near( m.getFloat( 1 ), 0.8 )
       && m.getString( 2 ) == "legato", 17 );
// type mismatches and missing arguments
check( m.getString( 0 ) == "" && m.getFloat( 2 ) == 0 && m.getInt( 5 ) == 0
       && m.getInt( -1 ) == 0, 18 );
// getInt truncates
check( pin.recv( m ) && m.getInt( 0 ) == 61 && m.numArgs() == 1, 19 );
// 'other' and 'cc' did not reach pin
check( !pin.recv( m ), 20 );
// pin2 got both notes and the cc, in order
check( pin2.recv( m ) && m.getInt( 0 ) == 60, 21 );
check( pin2.recv( m ) && m.getInt( 0 ) == 61, 22 );
check( pin2.recv( m ) && m.name() == "cc" && m.getInt( 0 ) == 7, 23 );
check( !pin2.recv( m ), 24 );

// --- Host.msg overloads: the patch prints each as BIND-MBANG/MSYM/MLIST ----
Host.msg( "mbang" );
Host.msg( "msym", "hello" );
Host.msg( "mlist", [1.0, 2.5, -3.0] );
float mempty[0];
Host.msg( "mempty", mempty );     // arrives as 'msg mempty', so bang

Host.msg( "done", failures );
