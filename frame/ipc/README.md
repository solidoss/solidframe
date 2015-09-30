# SolidFrame IPC module

## TODO

* Add support for canceling pending send Messages
* Prevent DOS made possible by sending lots of KeepAlive packets
	* ? set timer for non-active connections ?
	* ? count the number of received keep alive packets per interval ?
* Add support for "One Shot Delivery"
	* Add OneShot(/Send/Delivery)Flag
	* Normally, ipc service will keep on trying to send a message that:
		* Either is Idempotent and no response was received
		* Or it was not (partially) sent at all
	* OneShot Messages will fail after the first try
	* The Service will wait for the peer side to become available then send the messages.
* Add support for SSL
