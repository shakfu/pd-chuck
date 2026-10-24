// run by tests/test_bind_send.pd ('make test-bind'): Host.send* without
// 'reply 1'. the patch prints what reaches each [r bt_*]

Host.sendFloat( "bt_none", 1 );   // no [r bt_none]: skipped
Host.sendFloat( "bt_r2", 7 );
Host.sendBang( "bt_b" );
Host.sendSymbol( "bt_s", "hello" );
Host.sendList( "bt_l", [1.0, 2.5, -3.0] );
float empty[0];
Host.sendList( "bt_le", empty );
float big[40];
Host.sendList( "bt_lt", big );    // cut to 32
10::ms => now;
// one block: 256 queued, 44 dropped
for( 0 => int i; i < 300; i++ ) Host.sendFloat( "bt_n", i );
