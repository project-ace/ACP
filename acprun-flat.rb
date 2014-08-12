#!/usr/bin/ruby

require 'resolv'

$SMSIZE_INT    = 2048
$MAX_PORTID    = 64000

smsize_int     = $SMSIZE_INT
file_nodenames = ARGV[ 0 ]
nprocs         = ARGV[ 1 ]
dir_comm       = ARGV[ 2 ]
name_comm      = ARGV[ 3 ]

nprocs_int     = nprocs.to_i
comm           = dir_comm + "/" + name_comm

#######################################################
#######################################################
#######################################################
def query_dst_ports_ipaddrs ( myaddrs, myports )
    dst_ports = []
    dst_addrs = []
    nprocs    = myaddrs.length
    for ip in 0...(nprocs - 1) do
        dst_ports[ ip ] = myports[ ip + 1 ]
        dst_addrs[ ip ] = myaddrs[ ip + 1 ]
    end
    dst_ports[ nprocs - 1 ] = myports[ 0 ]
    dst_addrs[ nprocs - 1 ] = myaddrs[ 0 ]
    return dst_addrs, dst_ports
end

def split_udpports ( line )
    len = line.length
    str = line[ 0...(len-1) ]
    ports = []
    while pos = str.index( "udp" ) do
        len  = str.length
        str  = str[ (pos+3)...(len-1) ]
        port = (str.split)[ 2 ]
        ports << port
        #port_tmp = (str.split)[ 2 ]
        #len  = port_tmp.length
        #pos  = port_tmp.rindex(":")
        #if pos != nil && len != nil && pos >= 0 && len >= 1 then
        #    port = port[ pos...(len-1) ]
        #    p str, port_tmp, port
        #    ports << port
        #end
    end
    return ports
end

def query_all_udpports ( nodenames_uniq )
    #p "nodenames_uniq", nodenames_uniq
    lines = []
    ports = Hash.new
    ports_dup = Hash.new
    for ip in 0...nodenames_uniq.length
        comm_str = "netstat -ano | egrep 'tcp|udp'"
        comm_netstat = sprintf( "ssh %s eval \"%s\"", nodenames_uniq[ ip ], comm_str )
        line = (%x[ #{comm_netstat} ])
        lines << line
        ports [ nodenames_uniq [ ip ] ] = split_udpports( line )
        #p comm_netstat, line, ports
    end
    for ip in 0...nodenames_uniq.length
        node = nodenames_uniq [ ip ]
        ports_dup [ node ] = []
        for i in 0...ports[ node ].length
            port_pre = ports[ node ][ i ]
            pos  = port_pre.rindex(":") + 1
            len  = port_pre.length
            port = port_pre[ pos...len ]
            ports_dup [ node ] << port.to_i
            #printf ( "%16s%10d%10d => %s\n", port_pre, pos, len, port ) ;
        end
        ports_dup [ node ].uniq.sort
    end
    #p lines, ports
    return ports_dup
end

def query_proc_multiplicity ( nodenames, nodenames_uniq )
    nprocs_multiplicity_node = Hash.new
    for ip in 0...nodenames_uniq.length
        node_uniq = nodenames_uniq [ ip ]
        count = 0
        nodenames.each do
            |node|
            if node == node_uniq then
                count = count + 1
            end
        end
        nprocs_multiplicity_node [ node_uniq ] = count ;
    end
    return nprocs_multiplicity_node
end

def query_ports_notused( ports_used, nprocs_multiplicity_node, max_portid )
    ports_notused = Hash.new
    offset = 0
    ports_used.each do
        |node, ports|
        #p ports_used[ node ].length, ports_used[ node ].sort
        nprocs_node = nprocs_multiplicity_node [ node ]
        maxportid_used = ports_used [ node ].sort.last
        ports_notused [ node ] = []
        max_portid.downto( maxportid_used + 1 ) do
            |id|
            if nprocs_node <= 0 then
                break
            end
            ports_notused [ node ] << id #+ offset
            nprocs_node = nprocs_node - 1
            offset = offset + 1
        end
        #ports_notused [ node ] = ports_notused [ node ].reverse
        ports_notused [ node ] = ports_notused [ node ]
    end
    return ports_notused
end

#######################################################
#######################################################
#######################################################
nodenames = []
IO.foreach( file_nodenames ) do
    |nodename|
    nodename.chop!
    nodenames << nodename
end
p nodenames

#######################################################
nodenames_uniq = nodenames.uniq
nprocs_multiplicity_node = query_proc_multiplicity( nodenames, nodenames_uniq )
ports_used = query_all_udpports( nodenames_uniq )
ports_ok   = query_ports_notused( ports_used, nprocs_multiplicity_node, 64000 )

p "ports_ok", ports_ok

#######################################################
#######################################################
#######################################################
p smsize_int, file_nodenames, nprocs
p dir_comm, name_comm, comm

#ports_ok.each do
#    |node, ports|
#    p node, ports
#end
#
#######################################################
ipaddrs = []
nodenames.each do
    |nodename|
    ipaddr = Resolv.getaddress( nodename )
    ipaddrs << ipaddr
end
p ipaddrs

#######################################################
ranks_int = []
for ip in 0...nprocs_int do
    ranks_int << ip
end
ports_int = []
for id in 0...ports_ok.length do
    node = nodenames [ id ]
    ports_int.concat( ports_ok [ node ] )
end

p ipaddrs, ports_int
daddrs, dports_int = query_dst_ports_ipaddrs( ipaddrs, ports_int )
p daddrs, dports_int

comms_ssh = []
for ip in 0...(nprocs_int - 1) do
    comms_ssh << sprintf( "ssh %s %s %d %d %d %d %d %s > out%03d.log 2>&1 &",
                          nodenames[ ip ], comm, ranks_int[ ip ], nprocs_int,
                          smsize_int, ports_int[ ip ], dports_int[ ip ], daddrs[ ip ], ip )
end
ip = nprocs_int - 1
comms_ssh << sprintf( "ssh %s %s %d %d %d %d %d %s > out%03d.log 2>&1",
                      nodenames[ ip ], comm, ranks_int[ ip ], nprocs_int,
                      smsize_int, ports_int[ ip ], dports_int[ ip ], daddrs[ ip ], ip )

for ip in 0...nprocs_int do
    p comms_ssh [ ip ]
end

##########################################################
##########################################################
##########################################################
for ip in 0...nprocs_int do
    %x[ #{comms_ssh[ ip ]} ]
end

##########################################################
##########################################################
##########################################################
#sleep 5
#cp /dev/null result.log
#for CHID in $LIST_CHID
#do
#    cat out${CHID}.log >> result.log
#done
#
