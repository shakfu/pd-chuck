// Host.msg(name, value) -> chuck~ reply outlet: msg <name> <value>
// in Pd: send 'reply 1' to [chuck~] first, then
// [chuck~] rightmost outlet -> [route msg] -> [route freq]
// Host.sendBang/Float/Symbol/List(receiver, ...) -> every [r <receiver>],
// no 'reply 1' needed: try [r tick], [r freq], [r range], [r pair]

SinOsc s => dac;
0.2 => s.gain;

while( true )
{
    Math.random2f( 200, 800 ) => float f => s.freq;
    Host.msg( "freq", f );
    Host.sendBang( "tick" );
    Host.sendFloat( "freq", f );
    Host.sendSymbol( "range", f < 500 ? "low" : "high" );
    Host.sendList( "pair", [f, s.gain()] );
    250::ms => now;
}
