* Master branch currently contains 2.1.0.0 [beta 1 - testnet only] *

---------------------------------------------------------------------------------------------------------------------------------------

Cloak uses the Enigma system in order to faclilate private/secure transactionns. 

_How does this work?_

_**CloakShield - Node to node communications**_

On startup, each Cloak wallet generates a [NID_secp256k1] keypair (Cloaking Encryption Key / CEK) to enable them to derive ad-hoc secrets using ECDH with their private key and the recipient's public key. This communication forms the basis on all node-to-node communications relating to Enigma. See 'src/enigma/cloakshield.h/.cpp' for more information on this. This ECDH based encrypted communication is also utilized for onion-routed data, which is handled by CloakShield. CloakShield is a nice name for this collective functionality relating to encrypted node communications. 

When onion routing is enabled, the client will attempt to construct a valid onion route for the data using the list of Enigma peers that it is aware of. The node may not have a direct connection to the Enigma peers, but that is not necessary as CloakData (data packed for routing with CloakShield) packets are relayed peer-to-peer. An onion route will typically consist of 3 distinct routes to the destination node, with 3 node hops per route. Multiple routes are used to cope with situations where a routing node drops offline. 

Nodes periodically send out an Enigma Announcement (src/enigma/enigmaann.h) to peers to advertise their services for onion routing. Other nodes on the network store the announcements (until they expire or are replaced with an update) and use them to construct the onion routes. 

**NOTE:** The annoucement code could be improved to reduce traffic on the network. At present, re-announcements are sent periodically. It would be prudent to extend this and only send small signed "I'm still alive!" messages (at least as long as the underlying associated encryption key data remains the same). 

**IMPROVEMENT:** The annoucement system is a potential DoS target as there is currently very little computational cost to generating an Enigma Announcement. To mitigate this, a proof-of-work should be generated for the annoucement and transmitted with it. Peers can then quickly and cheaply verify the proof and dicard any bad data.


_**Enigma - Transaction Process**_

When a node sends funds via Enigma to an stealth address, the following happens:

1. Sender generates inputs to cover amount sent, enigma reward (sent * 1.8%) and network fee (unknown at present, so ample reserved).
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
14. Cloakers check the TX to ensure their inputs are present and correct and that there are one-time payment addresses linked to one of thier stealth addresses with payment that exceeds the input amount.
15. Cloakers sign or reject the TX and send signatures to Sender.
16. Sender collates the signatures and transmits the finalized, signed TX to the network.
17. Nodes scan incoming transactions for stealth payments and Enigma payments and detect any payments or change.

---------------------------------------------------------------------------------------------------------------------------------------


CloakCoin (CLOAK) Release

CloakCoin is a cool new crypto currency that will feature a uniquely implemented anonymization feature that uses exchanges on the back end and a decoupled transaction flow architecture.

This wallet supports the staking=0 option in the CloakCoin.conf file to disable the stake miner thread for pool and exchange operators.

Current dependencies:

qt 5.5.1

openssl v1.0.2g

boost v1.57.0

Curl v7.40.0

leveldb (Bitcoin Fork) v1.2

berkeley DB v4.8.30

libpng v1.6.16

libEvent v2.0.21

miniupnpc v1.9

protobuf v2.6.1

qrencode v3.4.4
 

