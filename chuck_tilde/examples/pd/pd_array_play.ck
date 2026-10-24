// HostBufPlay: loop array ckbuf. follows the array as it is refilled,
// resized, or deleted (silence).
// in Pd: [msg rate 0.5( -> [chuck~] sets the rate; negative plays backward

HostBufPlay p => dac;
0.5 => p.gain;
p.name( "ckbuf" );
1 => p.loop;

HostIn pin;
pin.addAddress( "rate" );
HostMsg m;
while( true )
{
    pin => now;
    while( pin.recv( m ) ) m.getFloat( 0 ) => p.rate;
}
