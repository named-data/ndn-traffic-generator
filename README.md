Traffic Generator For NDN (ndn-traffic)
======================================

This is an application tool developed over ndn-cpp APIs for TLV packet
based communication over NDN configured network. The NDN network has to
be first created with 'ndnx' and 'ndnd-tlv'. This is followed by environment
security setup with 'ndn-cpp-security-tools'. The application also requires
installation of ndn-cpp and CPP boost libraries. The installations need to
follow a strict order as the ndnd

To run the following must be ensured(FOLLOW ORDER STRICTLY)

1. Install ndnx (install all necessary dependencies)
2. Install CPP Boost Library > 1.47

        sudo apt-get install libboost1.48-all-dev

3. Install ndn-cpp-dev (install all necessary dependencies except boost)
4. Install ndnd-tlv install all necessary dependencies except boost)
5. Install and Configure Security Environment with ndn-cpp-security-tools

-----------------------------------------------------

## 1. Compile And Installation Instructions: ##

    git clone git://github.com/jeraldabraham/ndn-traffic ndn-traffic
    cd ndn-traffic
    ./waf configure 
    ./waf
    sudo ./waf install

## 2. Tool Run Instructions & Command Line Options: ##

     TBU

## 3. Sample Run Instructions ##

     TBU

* Use command line options shown above to adjust traffic configuration.
