# Development ideas

- Measure AF_PACKET ring capture and batching under sustained GRE traffic.
- Evaluate native nftables sets for large MAC populations and rapid provisioning.
- Design explicit authorization and controlled MAC mobility policies.
- Add configurable idle expiry with kernel and firewall ownership tracking.
- Insert or overwrite DHCP option 82 in softgred (remote-id = AP IPv4, circuit-id = tunnel
  VLAN interface) for lease-time policy without relying on APs to strip client copies.
- Show DHCP lease, AP and tunnel bindings from the lease hook in the dashboard.
