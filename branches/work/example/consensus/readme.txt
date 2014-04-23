This document contains design ideas about the replicated object concept

Working on the following scenario:

	- a client object C1 requesting a write opperation RC1 from a three time replicated object: Sx, Sy, Sz.
	
Some ideas:

* S* must associate an internal state Ss to RC1, and it MUST be the same for every replica.
* the smaller server object the bigger the decision power (i.e. Sx in our case has the decision, if Sx is out, Sy takes the decision ... etc).
* the requests from a client are received on S* in the same order they were sent (this is ensured by IPC).
* the client randomly/algorithmically (can continuously permute Sx,Sy,Sz! so everyone can take charge on decision) decides an order for Sx,Sy,Sz for every request it sends to S*. For simplicity we say: Sx<Sy<Sz.

1) First case - every process involved is up and running.
--------------------------------------------------------

* Sx receives RC1:
	* associate RC1 to its current internal state Ssx and send LockSet(RC1, Ssx) to Sy and Sz.
	* waits for OKs from Sy and Sz
	* executes RC1 and sends response to C1

* Sy receives RC1:
	* wait an amount of time TW in which in normal conditions, if Sx would be alive, would have sent a LockSet signal.
	* if received LockSet 
		* then
			* wait for RCt if not received allready, and associates Ssy == Ssx (?) to RCt
			* send ok to Sx
			* execute RCt and sends response to Ct
			* execute RC1 and sends response to C1
		* else // no response from Sx in TW
			* send to Sx a LockRequest(RC1, Ssy)
			* if Sx responds LockOk
				* then //Sx didn't receive RC1 and there was no other pending signal
					* execute RC1 and sends response to C1
			* if Sx responds LockSet((RCt, Ssx),(RC1, Ssx + 1)...)
				* then 
					...
			* if no response from Sx
				* then 
					* mark Sx as disconnected
					* send LockSet(RC1, Ssy) to Sz
					* wait OK from Sz
					* executes RC1 and sends response to C1

* Sz receives RC1:
	* wait an amount of time 2TW in which in normal conditions, if Sx or Sy would be alive, would have sent a LockSet signal.
	* if received LockSet((RCt, Ssx),(RC1, Ssx + 1)...)
		* then
			* wait for RCt if not received allready, and associates Ssz == Ssy == Ssx (?) to RCt
			* send ok to Sx or Sy
			* execute RCt and sends response to Ct
			* execute RC1 and sends response to C1
		* else
			* send to Sx and Sy a LockRequest(RC1, Ssz)
			
	
