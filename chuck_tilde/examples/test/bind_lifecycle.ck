// run by tests/test_bind_lifecycle.pd ('make test-bind'): two HostIn objects
// on 'w', one in a sporked child, both blocked when the shreds are removed

fun void child()
{
    HostIn pin;
    pin.addAddress( "w" );
    while( true ) pin => now;
}
spork ~ child();

HostIn pin;
pin.addAddress( "w" );
while( true ) pin => now;
