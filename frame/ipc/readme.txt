Some ideas on the design

* There are three basic way to send a message to a remote peer:
	1) Use the peer name string.
	2) Use a previously aquired sessionid.
	
Peers will only be identified by a name.
If there is no Session to peer, the peer name is first resolved.

Configuration will allow specifying a resolving function.

Requirements for protocol message

Scenario

	ipc::Message	<-	consensus::ReadMessage	<-	remote_file::ListMessage	<-	remote_file::ListMessageEx


