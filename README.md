# atmega-network
# Doug Powers, 2013

Overview
---------------------------------------------------------------------------
This is a network library for the Arduino and atmega chipsets and provides
support for the Ethernet, IP, TCP, and UDP protocols.  Support for ARP
and DNS are provided as is support for SMTP and HTTP.

The sourcecode for this library is made available under GPLv2. Please
see LICENSE file for more details.

Code is maintained on github and contributions are welcome.
  
       https://github.com/dougkpowers/atmega-network/



Network Device Hardware Support
---------------------------------------------------------------------------
The motivation for this library was to create a very inexpensive way
to prototype network-capable hardware using an Atmega chipset (for example,
network-capable Arduino projects).  An Arduino ethernet shield retails for $30
USD+.  However, ethernet controllers are available for a few dollars.

This library comes pre-packaged with a driver for the ENC28J60 ethernet 
controller.  However, drivers for other controllers may be easily
incorproated.  The ENC8J60 ethernet controller is a very inexpensive
ethernet controller available for a few dollars (you should be able to find it
for less than $5 USD online).

The datasheet for the controller may be found here:
   http://ww1.microchip.com/downloads/en/DeviceDoc/39662c.pdf

Additional erratas and other production information can be found here:
   http://www.microchip.com/wwwproducts/Devices.aspx?dDocName=en022889

Incredible amounts of credit go to Guido Socher who first wrote an
ENC28J60 interface driver for the Arduino.  This driver is largely
based on his implementation.
  
In turn, his implementation was based on the enc28j60.c file
from the AVRlib library by Pascal Strong.  For AVRlib see
http://www.procyoneengineering.com/


Performance Constraints
---------------------------------------------------------------------------
Network Speed

  Don't expect to see breathtaking network throughput with this library
  on an Arduino.  An ATmega328P (the Arduino UNO chipset) has only 2KB
  of SRAM, which limits the size of the network buffer, and in turn, creates
  constraints on the size of packets.  Smaller packet sizes create a more
  "chatty" TCP connection with more ACKs per connection than one would
  normally expect to see.

  If you are simply trying to transmit analog or digital reads from your
  Arduino to a network connected PC, I recommend using UDP packets.  While
  you risk losing packets during transmission, you should be able to transmit
  far more reads over the same time period since the TCP protocol has to wait
  for ACKs over the network before transmitting the next packet.

Footprint

  The library was built to flexibly use protocols as needed per project.
  As a result, the memory footprint is not optimized for any particular
  or specific network use.

  The more protocols you use from this driver, the larger the footprint
  of the resulting binary.  An Atmega328P (the Arduino UNO chipset) has
  only 32KB of flash memory, and you will use a considerable amount of it
  if you use a full-stack TCP protocol such as SMTP or HTTP.  Depending
  on the size of your project, you may exceed the 32KB flash memory
  constraint quickly.

  You can significantly limit the size of your footprint by using UDP rather
  than TCP.  On the other hand, if you use TCP and try to send an email from 
  your Arduino, you will have very little space left for your project.  If
  you do need more space, consider a chip with more Flash Memory such
  as the ATmega64.

      http://www.atmel.com/products/microcontrollers/avr/megaavr.aspx

Flow control
---------------------------------------------------------------------------

    You can control when you send data out on the network, but you have
    no control over when data will arrive.  Therefore, in every 'loop()' 
    of your program, you must check for incomming data and process it.
    At the Ethernet level, a single transmission of data is called a 'frame.'
    This is the smallest chunk of data that is sent or received.  On each
    loop() of our program, we therefore must check for an incoming Ethernet 
    Frame and process it.

    There are different protocols that run over Ethernet and how we handle
    the frame depends on the protocol.  For example, the ARP protocol is used
    to find MAC addresses for a given IP address, and the ARP protocol
    has its own frame type.  Likewise, the IP protocol is used to send data 
    to a specific IP address (MAC address on Ethernet) and it also has its
    own frame type.  We tell the Ethernet controller about our ability to
    process certain types of frames by giving it the handler for each frame 
    type.  For example:

        arp = new ARPHandler(myip,2,control);
        /\                    /\  /\   /\
        |                     |   |    |
        |                     |   |    The ethernet controller to register
        |                     |   |    ourself with
        |                     |   |
        |                     |   The size of the ARP routing table
        |                     |
        |                     |
        |                     Our IP so it can respond to requests for 
        |                     those looking for our MAC address
        |
        ----a pointer to our ARP Handler

    Now, when a frame comes in, the controller will pass it to the ARPHandler
    for processing.

    Similary, we can create an IPHandler for handling of frames with IP packet
    data: 

        ip = new IPHandler(myip,gwip,subnetmask,arp,control);
        /\                  /\  /\   /\         /\      /\
        |                   |   |    |          |       |  
        |                   |   |    |          |       |
        |                   |   |    |          |       The ethernet controller
        |                   |   |    |          |       to register ourself with
        |                   |   |    |          |
        |                   |   |    |          The arp handler to use when
        |                   |   |    |          we need to find the MAC address
        |                   |   |    |          for an IP
        |                   |   |    |
        |                   |   |    Subnet mask
        |                   |   |
        |                   |   Gateway IP
        |                   |   
        |                   Our IP Address
        |
        ----a pointer to our ARP Handler

    The protocol stack is built up in this way, so that an incoming Ethernet
    frame is handed to the appropriate 'Handler.'  In turn the IP Handler
    will unwrap the IP frame into an IP packet and pass the packet to
    a packet handler, which knows how to process packets for that protocol
    (TCP vs UDP).

       udp = new UDPHandler(ip, 1);
        /\                  /\  /\  
        |                   |   |___the number of receivers waiting for 
        |                   |       Datagrams   
        |                   |   
        |                   |--- the IP handler to register ouself with
        |                   
        ---a pointer to our UDP handler

    In the example above, we reference a 'receiver.'  A receiver, in this
    case, is just another handler.  When a UDP Datagram arives, what will
    the UDPHandler do with it?  It will pass it to a Receiver, such as
    a DNS, which operates over UDP:

        dns = new DNSHandler(udp,dnsip,1);
        /\                    /\  /\   /\
        |                     |   |    |___cache capacity, how many hostnames
        |                     |   |        to IPs should we keep in memory?
        |                     |   |    
        |                     |   |---IP address of DNS server
        |                     |
        |                     UDPHandler to register ourself with
        |   
        --- a pointer to our DNS handler
   
    We can now lookup IPs for hostnames and get a response:

        loop(){
            ...
            //see if we can resolve
            uint8_t error;
            uint8_t* remoteIP = dns->resolve(remoteDomain,&error);

            if (error == PENDING){ 
                //still waiting for a response
                //on each loop, keep calling dns->resolve(..) until we
                //get a resonse
            }
            else if (error == NO_ERROR && remoteIP != NULL){
                //we have the IP of the remote hostname
            }
        }   

     You may wonder, why are we calling resolve(..) in the loop?  On an
     Arduino, we do not have the benefit of threads and blocking calls.  
     The first time we call resolve(..) the DNSHandler simply sends out a 
     request.  On subsequent calls, it knows whether the response is pending
     and will not send out another request unless the original request times
     out.  Eventually, after a few iterations of the loop(), the resolve(..)
     function will return the IP of the remote host.


Example: Sending Data from the Arduino using UDP Datagrams
---------------------------------------------------------------------------
Step #1:  Include the header files for each of the protocols you expect
          to use:

    #include <ENC28J60Driver.h>     -- The driver for the ENC28J60 chipset
    #include <EtherControl.h>       -- Manages the interface to the driver
    #include <ARPHandler.h>         -- Manages ARP protocol
    #include <IPHandler.h>          -- Manages the IP protocol
    #include <UDPHandler.h>         -- Manages the UDP protocol

Step #2:    Delcare your network configuration

    static byte myip[] = { 192,168,1,2 };
    static byte gwip[] = { 192,168,1,1 };
    static byte dnsip[] = {8,8,8,8};        //Google DNS
    static byte dnsipbackup[] = {8,8,8,8};  //Google DNS

    static byte subnet[] =     {192,168,1,0};
    static byte subnetmask[] = {255,255,255,0};

    //must be unique on your local network
    static byte mymac[] = { 0x74,0x69,0x69,0x2D,0x01,0x01 };

Step #3:   Declare your driver and protocol handlers:

    ENC28J60Driver* driver;
    EtherControl* control;
    ARPHandler* arp;
    IPHandler* ip;
    UDPHandler* udp;

Step #4:   In your setup() routine, instantiate your protocol handlers,
           and request the MAC address of your gateway:

    void setup() {

       //instiate your driver with your mac address
       driver = new ENC28J60Driver(mymac); 

       //tell the controller which driver you are using
       control = new EtherControl(driver);
 
       //create a handler for the ARP ethernet frames
       arp = new ARPHandler(myip,2,control);

       //create a handler for IP packets
       ip = new IPHandler(myip,gwip,subnetmask,arp,control);

       //create a handler for UDP based IP packets
       udp = new UDPHandler(ip,1);

       //request the mac address of your gateway
       arp->requestMACAddress(gwip);
    }


Step #5:    In your loop(), you must look for new ethernet frames
            and ensure they are processed.  Therefore, the first
            line of your loop() function should process Ethernet frames
            on the Ethernet controller

    void loop(){
      control->processFrame();
    }


Step #6:    In the first few cycles of our loop, we will simply be trying
            to determine the MAC address of our gateway.  Until we
            have an Ethernet route to our gateway, we can't do anything
            on the network.  In this case, we simply return from the loop() 
            function;

    void loop(){
      control->processFrame();

      //if we have no route to the gateway, exit the loop
      if (arp->getMACAddress(gwip) == NULL){
        return;
      }
    }


Step #7:    Send out some UDP packets

    void loop(){
      control->processFrame();

      //if we have no route to the gateway, exit the loop
      if (arp->getMACAddress(gwip) == NULL){
        return;
      }

      //since we have the MAC address fo the gwip, let's send
      //some UDP packets out
      udp->sendDatagram(destinationIP, destinationPort, 
		        sourcePort, "I am here!");

    }

Further Reading
---------------------------------------------------------------------------
When the basics of flow control and sending UDP packets are understood,
you should be in good shape to explore the rest of the library on your own.
Comments are not the most detailed, but if you're looking to contribute...

Best of luck,

Doug



