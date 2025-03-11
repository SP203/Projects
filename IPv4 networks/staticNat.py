from mininet.net import Mininet
from mininet.node import Node
from mininet.topo import Topo
from mininet.cli import CLI
from mininet.log import setLogLevel

class TwoRoutersTopo(Topo):
    """Topologia z dwoma hostami i dwoma routerami."""
    def build(self):
        # Tworzymy hosty dla pierwszego routera
        h1 = self.addHost('h1', ip='192.168.1.2/24')
        h12 = self.addHost('h12', ip='192.168.1.3/24')
        h13 = self.addHost('h13', ip='192.168.1.4/24')
        h_global = self.addHost('h_global', ip = '4.4.4.2/24')

        #Tworzymy hosty dla drugiego routera
        h2 = self.addHost('h2', ip='192.168.2.2/24')
        h22 = self.addHost('h22', ip='192.168.2.3/24')
        h23 = self.addHost('h23', ip='192.168.2.4/24')

        #Tworzymy hosty dla trzeciego routera
        h3 = self.addHost('h3', ip='192.168.3.2/24')
        h32 = self.addHost('h32', ip='192.168.3.3/24')
        h33 = self.addHost('h33', ip='192.168.3.4/24')

        # Tworzymy routery
        r1 = self.addNode('r1',  ip='192.168.1.1/24')
        r2 = self.addNode('r2',  ip='10.0.0.2/24')
        r3 = self.addNode('r3', ip= '192.168.3.1/24')

        #Tworzymy switche
        s1 = self.addSwitch('s1')
        s2 = self.addSwitch('s2')
        s3 = self.addSwitch('s3')

        #Laczymy  hosty z routerami i routery miedzy soba
        self.addLink(h1, s1)
        self.addLink(h12, s1)
        self.addLink(h13, s1)

        self.addLink(s1, r1)
        self.addLink(h_global, r1)
        self.addLink(r1, r2)

        self.addLink(r2, s2)

        self.addLink(s2, h2)
        self.addLink(s2, h22)
        self.addLink(s2, h23)

        self.addLink(h3, s3)
        self.addLink(h32, s3)
        self.addLink(h33, s3)

        self.addLink(s3, r3)

        self.addLink(r3, r1)
        self.addLink(r3, r2)


def run():
    setLogLevel('info')
    topo = TwoRoutersTopo()
    net = Mininet(topo=topo)
    net.start()

    h1, h2, h12, h13, h_global, h22, h23, h3, h32, h33  = net.get('h1', 'h2', 'h12', 'h13', 'h_global', 'h22', 'h23', 'h3', 'h32', 'h33')
    r1, r2, r3 = net.get('r1', 'r2', 'r3')

    #wlaczanie ipv4
    r1.cmd('sysctl net.ipv4.ip_forward=1')
    r2.cmd('sysctl net.ipv4.ip_forward=1')
    r3.cmd('sysctl net.ipv4.ip_forward=1')

    # Konfiguracja interfejsow routerow
    r1.cmd('ifconfig r1-eth0 192.168.1.1/24')
    r1.cmd('ifconfig r1-eth1 4.4.4.1/24 up')
    r1.cmd('ifconfig r1-eth2 10.0.0.1/24 up')  # Polaczenie z r2 (cos nie dziala!)
    r1.cmd('ifconfig r1-eth3 10.0.2.1/24 up')

    r2.cmd('ifconfig r2-eth0 10.0.0.2/24')
    r2.cmd('ifconfig r2-eth1 192.168.2.1/24 up')
    r2.cmd('ifconfig r2-eth2 10.0.3.1/24 up')

    r3.cmd('ifconfig r3-eth0 192.168.3.1/24')
    r3.cmd('ifconfig r3-eth1 10.0.2.2/24 up')
    r3.cmd('ifconfig r3-eth2 10.0.3.2/24 up')


    # Dodanie regul NAT w routerze r1
    r1.cmd('iptables -t nat -A POSTROUTING -s 192.168.1.2 -j SNAT --to-source 1.1.1.1')
    r1.cmd('iptables -t nat -A PREROUTING -d 1.1.1.1 -j DNAT --to-destination 192.168.1.2')
    r1.cmd('iptables -t nat -A POSTROUTING -s 192.168.1.3 -j SNAT --to-source 1.1.1.2')
    r1.cmd('iptables -t nat -A PREROUTING -d 1.1.1.2 -j DNAT --to-destination 192.168.1.3')
    r1.cmd('iptables -t nat -A POSTROUTING -s 192.168.1.4 -j SNAT --to-source 1.1.1.3')
    r1.cmd('iptables -t nat -A PREROUTING -d 1.1.1.3 -j DNAT --to-destination 192.168.1.4')

    # Dodanie regul  NAT w routerze r2
    r2.cmd('iptables -t nat -A POSTROUTING -s 192.168.2.2 -j SNAT --to-source 2.2.2.1')
    r2.cmd('iptables -t nat -A PREROUTING -d 2.2.2.1 -j DNAT --to-destination 192.168.2.2')
    r2.cmd('iptables -t nat -A POSTROUTING -s 192.168.2.3 -j SNAT --to-source 2.2.2.2')
    r2.cmd('iptables -t nat -A PREROUTING -d 2.2.2.2 -j DNAT --to-destination 192.168.2.3')
    r2.cmd('iptables -t nat -A POSTROUTING -s 192.168.2.4 -j SNAT --to-source 2.2.2.3')
    r2.cmd('iptables -t nat -A PREROUTING -d 2.2.2.3 -j DNAT --to-destination 192.168.2.4')

    # Dodanie regul  NAT w routerze r3
    r3.cmd('iptables -t nat -A POSTROUTING -s 192.168.3.2 -j SNAT --to-source 3.3.3.1')
    r3.cmd('iptables -t nat -A PREROUTING -d 3.3.3.1 -j DNAT --to-destination 192.168.3.2')
    r3.cmd('iptables -t nat -A POSTROUTING -s 192.168.3.3 -j SNAT --to-source 3.3.3.2')
    r3.cmd('iptables -t nat -A PREROUTING -d 3.3.3.2 -j DNAT --to-destination 192.168.3.3')
    r3.cmd('iptables -t nat -A POSTROUTING -s 192.168.3.4 -j SNAT --to-source 3.3.3.3')
    r3.cmd('iptables -t nat -A PREROUTING -d 3.3.3.3 -j DNAT --to-destination 192.168.3.4')


    # Dodanie tras w routerach
    r1.cmd('ip route add 2.2.2.0/24 via 10.0.0.2')
    r1.cmd('ip route add 3.3.3.0/24 via 10.0.2.2')


    r2.cmd('ip route add 1.1.1.0/24 via 10.0.0.1')
    r2.cmd('ip route add 4.4.4.0/24 via 10.0.0.1')
    r2.cmd('ip route add 3.3.3.0/24 via 10.0.3.2')

    r3.cmd('ip route add 1.1.1.0/24 via 10.0.2.1')
    r3.cmd('ip route add 4.4.4.0/24 via 10.0.2.1')
    r3.cmd('ip route add 2.2.2.0/24 via 10.0.3.1')

    # Dodanie tras w hostach
    h1.cmd('ip route add default via 192.168.1.1')
    h12.cmd('ip route add default via 192.168.1.1')
    h13.cmd('ip route add default via 192.168.1.1')
    h_global.cmd('ip route add default via 4.4.4.1')

    h2.cmd('ip route add default via 192.168.2.1')
    h22.cmd('ip route add default via 192.168.2.1')
    h23.cmd('ip route add default via 192.168.2.1')

    h3.cmd('ip route add default via 192.168.3.1')
    h32.cmd('ip route add default via 192.168.3.1')
    h33.cmd('ip route add default via 192.168.3.1')

  # Uruchamiamy interaktywne konsole
    CLI(net)
    net.stop()

if __name__ == '__main__':
    run()

