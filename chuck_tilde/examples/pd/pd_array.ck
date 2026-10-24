// HostBuffer write: fill array ckbuf with a decaying 220 Hz tone.
// in Pd: [array define ckbuf 44100]; click it to see the result.
// reply outlet (needs 'reply 1') -> [route msg] -> [route written]

HostBuffer b;
b.name( "ckbuf" );
if( !b.ready() ) { <<< "no array named", b.name() >>>; me.exit(); }

b.frames() => int n;
second / samp => float sr;
for( 0 => int i; i < n; i++ )
{
    b.poke( i, Math.sin( 2 * pi * 220 * i / sr ) * Math.exp( -4.0 * i / n ) );
}
Host.send( "written", n );
