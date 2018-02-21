Cloak uses the Enigma system in order to facilitate private/secure transactions. 

_How does this work?_

_**CloakShield - Node to node communications**_

On startup, each Cloak wallet generates a [NID_secp256k1] keypair (Cloaking Encryption Key / CEK) to enable them to derive ad-hoc secrets using ECDH with their
private key and the recipient's public key. This communication forms the basis on all node-to-node communications relating to Enigma. See
'src/enigma/cloakshield.h/.cpp' for more information on this. This ECDH based encrypted communication is also utilized for onion-routed data,
which is handled by CloakShield. CloakShield is a nice name for this collective functionality relating to encrypted node communications.

When onion routing is enabled, the client will attempt to construct a valid onion route for the data using the list of Enigma peers that it is
aware of. The node may not have a direct connection to the Enigma peers, but that is not necessary as CloakData (data packed for routing with
CloakShield) packets are relayed peer-to-peer. An onion route will typically consist of 3 distinct routes to the destination node, with 3 node
hops per route. Multiple routes are used to cope with situations where a routing node drops offline.

Nodes periodically send out an Enigma Announcement (src/enigma/enigmaann.h) to peers to advertise their services for onion routing. Other nodes
on the network store the announcements (until they expire or are replaced with an update) and use them to construct the onion routes.

**NOTE:** The annoucement code could be improved to reduce traffic on the network. At present, re-announcements are sent periodically. It would
be prudent to extend this and only send small signed "I'm still alive!" messages (at least as long as the underlying associated encryption key
data remains the same).

**IMPROVEMENT:** The annoucement system is a potential DoS target as there is currently very little computational cost to generating an Enigma
Announcement. To mitigate this, a proof-of-work should be generated for the annoucement and transmitted with it. Peers can then quickly and
cheaply verify the proof and dicard any bad data.


_**Enigma - Transaction Process**_

When a node sends funds via Enigma to an stealth address, the following happens:

1. Sender generates inputs to cover amount sent, enigma reward (sent * (.2-1)%) and network fee (unknown at present, so ample reserved).
2. Sender generates a CloakingRequest object (containing unique stealth nonce for this request).
3. Sender generates 2 or 3 one-time stealth payment addresses using the recipients stealth address and splits the sent amount randomly between the addresses.
4. Sender onion routes CloakRequest to network. The request contains the 'send amount' so that Cloakers know how much to reserve.
5. Cloaker picks up CloakRequest and decides to participate.
6. Cloaker supplies X inputs to sender and a stealth address and stealth hash (for their change).
7. Cloaker sends CloakingAcceptResponse to Sender. This contains stealth address, stealth nonce and TX inputs.
8. Sender waits until enough Cloakers have accepted.
9. Sender creates Enigma transaction using own inputs and Cloaker inputs. Inputs are shuffled.
10. Sender creates TX ouputs for all Cloakers. The outputs randomly split their change and return it to them. This also allocates the cloaking reward to Cloakers.
11. Sender creates their own change returns for the Enigma TX. These are one-time stealth payment addresses.
12. The Sender calculates the network TX fee and subtracts this from their own change return.
13. The Sender sends the Enigma TX to the Cloakers for signing. 
14. Cloakers check the TX to ensure their inputs are present and correct and that there are one-time payment addresses linked to one of thier
stealth addresses with payment that exceeds the input amount.
15. Cloakers sign or reject the TX and send signatures to Sender.
16. Sender collates the signatures and transmits the finalized, signed TX to the network.
17. Nodes scan incoming transactions for stealth payments and Enigma payments and detect any payments or change. Keypairs and addresses are generated
for any matching payments and generated keys/addresses are saved to the local wallet.

_**How are stealth and Enigma transactions detected/received?**_

All incoming transactions are scanned. Stealth transactions are scanned for first (using the default ephemeral pubkey contained in a random OP_RETURN
TX output). After this, Enigma transactions are then scanned for. Enigma transactions also use the standard ephemeral pubkey, but payments use an additional
step involving a further derived key. Enigma outputs are generated using a hash of the ephemeral pubkey, a private stealth address hash and the output index.

When scanning for Enigma transactions, the zero-index payment addresses are generated for each owned stealth address
[HASH(ephemeral_pubkey, hash_stealth_secret, 0)]. If a match is found for the zero-index of a stealth address, additional addresses are generated for the
remaining indexes [num_tx_outputs] and these are scanned against to detect payments. See FindEnigmaTransactions in wallet.cpp for more info.

A similar scanning method is employed by Cloakers prior to signing an Enigma TX to ensure they are getting reimbursed correctly. See GetEnigmaOutputsAmounts
in wallet.cpp for more info.

_**New Codebase**_

The Cloak3 project contains a fork of the recent Litecoin codebase. This comes with segwit, soft-forks are all the latest Bitcoin goodies. Work was started
on porting Cloak to this codebase, but stopped due to lack of resources. The wallet does indeed boot now, but header syncing needs more additional work.
Older Cloak clients do not differentiate between PoS and PoW blocks when issuing headers, so this will need to be addressed before work can continue. A
potential solution would be to avoid header-sync when connected to an older Cloak node as the problem will lessen as clients update over time.

Compiling the new codebase is an absolute dream in comparison to the old/existing codebase. For this reason alone it may be worth switching to the new
codebase prior to the public sourcecode release.

_**Potential Improvements**_

* Include an encrypted payment ID in the OP_RETURN stealth output for Enigma transactions. This allows the receiver to use the payment ID as the address
name for the newly generated/detected one-time address. This would also help stores and exchanges track Enigma payments more easily.
* Include a proof-of-work for Enigma Announcements and Cloak Requests. These are 2 potential DoS targets and adding a PoW function would migigate this.

_**Security, Peer Review and Audit**_

The Enigma code has had little to no peer review to date. This makes it critical that the code is reviewed carefully before public release. DoS attacks
may hamper users, but stolen funds are the most important area to cover when checking the code.
* Is it possible to trick a Cloaker into signing a transaction that does not pay them correctly? 
* Is it possible for a Cloaker to hijack the transaction during sending to trick the sender prior to the Sender finalizing the transaction?
