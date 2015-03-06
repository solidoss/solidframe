Some ideas on the desing

* There are three basic way to send a message to a remote peer:
	1) Use the peer name string.
	2) Use the peer dotten ip address.
	3) Use a previously aquired sessionid.add_compile_options(
	
A sessionid represent a map [peer_name_string, peer_addr_string] -> [(secure_connections), (plain_connections)]
When sending a message with a peer_name_string, the peer_addr_string is computed after a connection is established from connection's peerAddress.
Worst case scenario:
	* sendMessage("name_for_ip_192_168_10_1", msgptr, ssnuid1)
	* sendMessage("192.168.10.1", msgptr, ssnuid2)
	
The problem is that the second send message comes after "name_for_ip_192_168_10_1" is registered on ssnuid1 but before peer_addr_string for ssnuid1 is computed so a new session will get registered although it should be the same as ssnuid1.

Solutions:
	1) Resolve "name_for_ip_192_168_10_1" synchronously in sendMessage.
	2) See it as a non-problem. Ok, there will be two sessions for the same destination.The problem here is - where to link an incomming connection from "192.168.10.1"?
	3) The second session will be a pointer to the first one.
		1)ipc::Service will have a "one thread" asio::Resolver so that all resolves go synchronously.
		2)The second sendMessage's "192.168.10.1" resolve will complete after the first one's. So, on completion, instead of creating a connection, we make the current session a pointer session, pointing to the "name_for_ip_192_168_10_1"
	